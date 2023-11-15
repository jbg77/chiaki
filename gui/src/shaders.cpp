// nodebug

static const char *p010le_shader_frag_glsl = R"glsl(
#version 150 core

uniform sampler2D plane1; // Y
uniform sampler2D plane2; // interlaced UV

in vec2 uv_var;
out vec4 out_color;

void main() {
    // Fetch the 16-bit Y, U, and V values
    uint y_full = uint(texture(plane1, uv_var).r * 65535.0);
    uint uv_full_r = uint(texture(plane2, uv_var).r * 65535.0);
    uint uv_full_g = uint(texture(plane2, uv_var).g * 65535.0);

    // Unpack the 10-bit Y, U, and V values from the 16-bit data
    float y = float(y_full >> 6) / 1023.0; 
    float u = float(uv_full_r >> 6) / 1023.0 - 0.5; 
    float v = float(uv_full_g >> 6) / 1023.0 - 0.5; 

    // BT.709 YUV to RGB conversion coefficients (adjust if needed)
    vec3 yuv = vec3(y, u, v);
    mat3 yuv2rgb = mat3(
        1.1643828125,  1.1643828125,  1.1643828125,
        0.0,           -0.2132486143,  2.1124017857,
        1.7927410714, -0.5329093286,  0.0
    );
    vec3 rgb = yuv2rgb * (yuv - vec3(0.0625, 0.5, 0.5));

    // Clamping the output color to the valid range
    rgb = clamp(rgb, 0.0, 1.0);

    out_color = vec4(rgb, 1.0);
}
)glsl";

/// debug
static const char *p010le_shader_frag_glsl = R"glsl(
#version 150 core

uniform sampler2D plane1; // Y
uniform sampler2D plane2; // interlaced UV

in vec2 uv_var;
out vec4 out_color;

void main() {
    // Debug color output
    vec3 debug_red = vec3(1.0, 0.0, 0.0);
    vec3 debug_green = vec3(0.0, 1.0, 0.0);
    vec3 debug_blue = vec3(0.0, 0.0, 1.0);

    // Fetch the 16-bit Y, U, and V values
    uint y_full = uint(texture(plane1, uv_var).r * 65535.0);
    uint uv_full_r = uint(texture(plane2, uv_var).r * 65535.0);
    uint uv_full_g = uint(texture(plane2, uv_var).g * 65535.0);

    if (y_full == 0u) {
        // Output red if Y is zero
        out_color = vec4(debug_red, 1.0);
        return;
    }

    // Unpack the 10-bit Y, U, and V values from the 16-bit data
    float y = float(y_full >> 6) / 1023.0;
    float u = float(uv_full_r >> 6) / 1023.0 - 0.5;
    float v = float(uv_full_g >> 6) / 1023.0 - 0.5;

    if (u == -0.5 && v == -0.5) {
        // Output green if U and V are at their lower bounds
        out_color = vec4(debug_green, 1.0);
        return;
    }

    // BT.709 YUV to RGB conversion coefficients (adjust if needed)
    vec3 yuv = vec3(y, u, v);
    mat3 yuv2rgb = mat3(
        1.1643828125,  1.1643828125,  1.1643828125,
        0.0,           -0.2132486143,  2.1124017857,
        1.7927410714, -0.5329093286,  0.0
    );
    vec3 rgb = yuv2rgb * (yuv - vec3(0.0625, 0.5, 0.5));

    if (all(lessThan(rgb, vec3(0.01)))) {
        // Output blue if RGB values are near zero
        out_color = vec4(debug_blue, 1.0);
        return;
    }

    // Clamping the output color to the valid range
    rgb = clamp(rgb, 0.0, 1.0);

    out_color = vec4(rgb, 1.0);
}

)glsl";

// blue
static const char *p010le_shader_frag_glsl = R"glsl(
#version 150 core

uniform sampler2D plane1; // Y
uniform sampler2D plane2; // interlaced UV

in vec2 uv_var;
out vec4 out_color;

void main() {
    // Sample the texture data
    vec4 y_tex = texture(plane1, uv_var);
    vec4 uv_tex = texture(plane2, uv_var);

    // Convert to 16-bit range and unpack to 10-bit range
    float y = floor(y_tex.r * 65535.0 + 0.5) / 65535.0;
    float u = floor(uv_tex.r * 65535.0 + 0.5) / 65535.0;
    float v = floor(uv_tex.g * 65535.0 + 0.5) / 65535.0;

    // Normalize the 10-bit range to 0-1 range
    y = (y * 1024.0) / 1023.0;
    u = (u * 1024.0) / 1023.0 - 0.5;
    v = (v * 1024.0) / 1023.0 - 0.5;

    // Debug: Highlight potential issues with data range
    if (y <= 16.0 / 1023.0) {
        out_color = vec4(1, 0, 0, 1); // Red for Y too low
        return;
    } else if (y >= (235.0 / 1023.0)) {
        out_color = vec4(0, 0, 1, 1); // Blue for Y too high
        return;
    }

    // YUV to RGB conversion
    float r = y + 1.402 * (v - 0.5);
    float g = y - 0.344136 * (u - 0.5) - 0.714136 * (v - 0.5);
    float b = y + 1.772 * (u - 0.5);

    // Clamp and output the color
    out_color = vec4(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), 1.0);
}
)glsl";

// blue2 
static const char *p010le_shader_frag_glsl = R"glsl(
#version 150 core

uniform sampler2D plane1; // Y plane
uniform sampler2D plane2; // UV plane

in vec2 uv_var;
out vec4 out_color;

void main() {
    // Sample the texture data
    vec4 y_tex = texture(plane1, uv_var);
    vec4 uv_tex = texture(plane2, uv_var);

    // Unpack the Y, U, and V values from the sampled data
    float y = y_tex.r * 1.1643 - 0.0627; // 16..235 range for Y
    float u = uv_tex.r - 0.5;             // 16..240 range for U
    float v = uv_tex.g - 0.5;             // 16..240 range for V

    // Debug: Check Y, U, V ranges
    if (y < 0.0 || y > 1.0) {
        out_color = vec4(1.0, 0.0, 1.0, 1.0); // Magenta for Y out of range
        return;
    }
    if (u < -0.5 || u > 0.5) {
        out_color = vec4(0.0, 1.0, 1.0, 1.0); // Cyan for U out of range
        return;
    }
    if (v < -0.5 || v > 0.5) {
        out_color = vec4(1.0, 1.0, 0.0, 1.0); // Yellow for V out of range
        return;
    }

    // Convert YUV to RGB
    float r = y + 1.596 * v;
    float g = y - 0.391 * u - 0.813 * v;
    float b = y + 2.018 * u;

    // Debug: Check RGB ranges before clamping
    if (r < 0.0 || r > 1.0) {
        out_color = vec4(1.0, 0.0, 0.0, 1.0); // Red for R out of range
        return;
    }
    if (g < 0.0 || g > 1.0) {
        out_color = vec4(0.0, 1.0, 0.0, 1.0); // Green for G out of range
        return;
    }
    if (b < 0.0 || b > 1.0) {
        out_color = vec4(0.0, 0.0, 1.0, 1.0); // Blue for B out of range
        return;
    }

    // Clamp and output the color
    out_color = vec4(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), 1.0);
}
)glsl";

// more
static const char *p010le_shader_frag_glsl = R"glsl(
#version 150 core

uniform sampler2D plane1; // Y plane
uniform sampler2D plane2; // UV plane

in vec2 uv_var;
out vec4 out_color;

const vec3 offset = vec3(-0.0627450980, -0.5, -0.5); // 16/255 for Y and 128/255 for U and V
const vec3 yuv2rgb_weights = vec3(1.164, 1.164, 1.164); // BT.601
const mat3 yuv2rgb_matrix = mat3(
    1.164,  0.000,  1.596,
    1.164, -0.391, -0.813,
    1.164,  2.018,  0.000
);

void main() {
    // Sample the texture data
    highp float y = texture(plane1, uv_var).r;
    highp vec2 uv = texture(plane2, uv_var).rg;

    // Convert the range from [16/255, 235/255] to [0, 1] for Y
    // and from [16/255, 240/255] to [-0.5, 0.5] for U and V
    highp vec3 yuv = vec3(y, uv) + offset;

    // Scale Y component from [16/255, 235/255] to [0, 1]
    yuv *= yuv2rgb_weights;

    // Perform YUV to RGB conversion
    highp vec3 rgb = yuv2rgb_matrix * yuv;

    // Clamp the output color to the valid range
    out_color = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)glsl";

// B&w 1
static const char *p010le_shader_frag_glsl = R"glsl(
#version 150 core

uniform sampler2D plane1; // Y plane

in vec2 uv_var;
out vec4 out_color;

void main() {
    // Sample the Y component
    float y = texture(plane1, uv_var).r;

    // The Y value needs to be scaled from the video range to the full [0,1] range
    // Assuming the Y component is already in the correct range [16/255, 235/255] for video
    float scaledY = (y - (16.0/255.0)) / ((235.0 - 16.0) / 255.0);

    // Set the RGB components to the Y value for a grayscale image
    out_color = vec4(scaledY, scaledY, scaledY, 1.0);
}
)glsl";

