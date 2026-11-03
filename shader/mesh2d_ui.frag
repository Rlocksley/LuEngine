#version 460
#extension GL_EXT_nonuniform_qualifier : require

// GUI shapes drawn with signed distance fields. Pair of mesh2d_ui.vert. Everything is described by Component::Mesh2D:
//
//   color   fill color. Untextured shapes: the top of the gradient. Textured shapes: tint (multiplied with the texture)
//   uvRect  untextured shapes: BORDER COLOR (rgba).  textured shapes: the uv rectangle, no border
//   params  x  corner radius in pixels
//           y  border width in pixels (shadows / glows: the margin = how much smaller the shadow-casting rectangle is than the
//              mesh, so that the blur has room around it; icons: stroke width)
//           z  softness in pixels. 0 = crisp, anti-aliased shape. > 0 = SOFT shape: a blurred rounded rectangle, a drop shadow or a
//              glow. Draw soft shapes with a blending pipeline (blendEnable, no depth write) registered AFTER the pipeline of
//              the crisp shapes.
//           w  kind * 4 + gradient, gradient in -1 .. 1. top = color * (1 + gradient), bottom = color * (1 - gradient)
//              (+ gradient: lighter at the top, - gradient: darker at the top = pressed / inset look).
//   kinds   0 rounded rectangle (a circle if radius = size / 2)     7 only the top corners rounded     8 only the bottom corners rounded
//           1 check mark   2 chevron down   3 chevron up   4 cross   5 chevron right   6 chevron left   (icons: drawn with the fill color)
//
// Use with: alpha to coverage + MSAA (GraphicsPipelineConfig::alphaToCoverageEnable, blendEnable = false, depth write on) for the
// crisp shapes: anti-aliased edges that do not depend on the draw order. And with blending (depth write off) for the soft shapes.
// Clipping: Component::Mesh2D::clip, as in mesh2d_clip.frag (with an anti-aliased edge).

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;
layout(location = 2) flat in uint inTextureId;
layout(location = 3) flat in uint inMeshId;
layout(location = 4) in vec2 inClip;
layout(location = 5) in vec2 inLocal;
layout(location = 6) flat in vec2 inSize;
layout(location = 7) flat in vec4 inParams;
layout(location = 8) flat in vec4 inBorder;

layout(location = 0) out vec4 outColor;

// the global texture array (TexturePackage)
layout(set = 0, binding = 3) uniform sampler2D textureArray[];

// ---- BEGIN SHADE ------------------------------------------------------------------------------------------------------------
// Plain GLSL without in / out parameters and swizzles: the offline preview tool compiles this block as C++ (glm).

// Signed distance to a rounded box (y down). r = radii of the top-left, top-right, bottom-right, bottom-left corner.
float sdRoundBox(vec2 p, vec2 halfSize, vec4 r) {
    float rr = p.x < 0.0 ? (p.y < 0.0 ? r.x : r.w) : (p.y < 0.0 ? r.y : r.z);
    vec2 q = vec2(abs(p.x) - halfSize.x + rr, abs(p.y) - halfSize.y + rr);
    return min(max(q.x, q.y), 0.0) + length(vec2(max(q.x, 0.0), max(q.y, 0.0))) - rr;
}

float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

// coverage of a pixel from the signed distance (negative = inside), aa = size of a pixel in the units of d
float coverage(float d, float aa) {
    return clamp(0.5 - d / aa, 0.0, 1.0);
}

// 0 .. 1 noise that breaks up the banding of gradients (interleaved gradient noise)
float noise(vec2 fragCoord) {
    return fract(52.9829189 * fract(dot(fragCoord, vec2(0.06711056, 0.00583715))));
}

// Distance to the icon `kind` (1 .. 6) in a box of the given size (the stroke is not subtracted).
float sdIcon(int kind, vec2 p, float s) {
    if (kind == 1) {            // check mark
        return min(sdSegment(p, vec2(-0.28, 0.02) * s, vec2(-0.08, 0.24) * s),
                   sdSegment(p, vec2(-0.08, 0.24) * s, vec2(0.30, -0.22) * s));
    }
    if (kind == 2 || kind == 3) {   // chevron down / up
        float f = kind == 2 ? 1.0 : -1.0;
        return min(sdSegment(p, vec2(-0.30, -0.12 * f) * s, vec2(0.0, 0.16 * f) * s),
                   sdSegment(p, vec2(0.0, 0.16 * f) * s, vec2(0.30, -0.12 * f) * s));
    }
    if (kind == 5 || kind == 6) {   // chevron right / left
        float f = kind == 5 ? 1.0 : -1.0;
        return min(sdSegment(p, vec2(-0.12 * f, -0.30) * s, vec2(0.16 * f, 0.0) * s),
                   sdSegment(p, vec2(0.16 * f, 0.0) * s, vec2(-0.12 * f, 0.30) * s));
    }
    // cross
    return min(sdSegment(p, vec2(-0.24, -0.24) * s, vec2(0.24, 0.24) * s),
               sdSegment(p, vec2(0.24, -0.24) * s, vec2(-0.24, 0.24) * s));
}

// color = fill (rgba), border = border color, tex = texture color (white if none), local = 0..1 in the shape (may be a bit outside),
// size in pixels, aa = size of a pixel in pixels, fragCoord for the dither. Returns straight (not premultiplied) rgba.
vec4 shadeUi(vec4 color, vec4 border, vec4 tex, vec4 params, vec2 local, vec2 size, float aa, vec2 fragCoord) {
    float wParam = params.w;
    int kind = int(floor((wParam + 2.0) / 4.0));
    float gradient = wParam - 4.0 * float(kind);
    float radius = params.x;
    float bw = params.y;
    float soft = params.z;

    vec2 p = (local - vec2(0.5)) * size;                 // pixels from the center of the shape
    float shade = 1.0 + gradient * (1.0 - 2.0 * clamp(local.y, 0.0, 1.0));
    float dither = (noise(fragCoord) - 0.5) / 255.0;

    vec4 fill = vec4(color.r * tex.r, color.g * tex.g, color.b * tex.b, color.a * tex.a);
    fill = vec4(fill.r * shade, fill.g * shade, fill.b * shade, fill.a);

    if (kind >= 1 && kind <= 6) {                        // icon
        float d = sdIcon(kind, p, min(size.x, size.y)) - bw * 0.5;
        return vec4(fill.r + dither, fill.g + dither, fill.b + dither, fill.a * coverage(d, aa));
    }

    // rounded box
    vec2 halfSize = size * 0.5;
    float r = radius;
    vec4 radii = vec4(r, r, r, r);
    if (kind == 7) radii = vec4(r, r, 0.0, 0.0);
    if (kind == 8) radii = vec4(0.0, 0.0, r, r);

    if (soft > 0.0) {                                    // blurred box: shadow / glow
        halfSize = halfSize - vec2(bw);
        float d = sdRoundBox(p, halfSize, radii);
        float t = clamp(0.5 - d / (soft * 2.0), 0.0, 1.0);            // 1 inside, 0 at 'soft' distance, smooth in between
        float a = t * t * (3.0 - 2.0 * t);
        return vec4(fill.r, fill.g, fill.b, fill.a * a);
    }

    float d = sdRoundBox(p, halfSize, radii);
    float outer = coverage(d, aa);
    float inner = bw > 0.0 ? coverage(d + bw, aa) : outer;
    float fa = fill.a;
    float ba = border.a;
    float ca = ba + (fa - ba) * inner;
    vec3 cp = vec3(border.r, border.g, border.b) * ba * (1.0 - inner) + vec3(fill.r, fill.g, fill.b) * fa * inner;
    vec3 rgb = ca > 1e-5 ? cp / ca : vec3(0.0);
    return vec4(rgb.r + dither, rgb.g + dither, rgb.b + dither, ca * outer);
}
// ---- END SHADE --------------------------------------------------------------------------------------------------------------

void main() {
    vec4 tex = vec4(1.0);
    vec4 border = inBorder;
    if (inTextureId != 0xFFFFFFFFu) {
        tex = texture(textureArray[nonuniformEXT(inTextureId)], inUv);
        border = vec4(0.0);                              // (uvRect is the uv rectangle of a textured shape)
    }

    vec2 p = (inLocal - vec2(0.5)) * inSize;
    float aa = max(max(length(dFdx(p)), length(dFdy(p))), 1e-4);

    vec4 c = shadeUi(inColor, border, tex, inParams, inLocal, inSize, aa, gl_FragCoord.xy);

    // clip rectangle (Mesh2D::clip) with an anti-aliased edge, in pixels of the screen
    vec2 fw = max(fwidth(inClip), vec2(1e-6));
    vec2 inside = min(inClip, vec2(1.0) - inClip) / fw;
    float clipAlpha = clamp(min(inside.x, inside.y) + 0.5, 0.0, 1.0);

    float alpha = c.a * clipAlpha;
    if (alpha < 0.004) {
        discard;     // nothing to draw here: must not write depth (it would hide shapes and text of lower layers)
    }
    outColor = vec4(c.rgb, alpha);
}
