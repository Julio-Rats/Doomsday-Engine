#version 330 core

uniform sampler2D uFramebuf;
uniform sampler2D uBloomFramebuf;
uniform float     uExposure;

DENG_VAR vec2 vUV;

void main(void) {
    vec3 hdr = texelFetch(uFramebuf, ivec2(gl_FragCoord.xy), 0).rgb +
               texture(uBloomFramebuf, vUV).rgb;

    // const float gamma = 0.5;
    // vec3 mapped = hdr * uExposure;

    const float gamma = 0.5; //1.4;
    vec3 mapped = 1.0 - exp(-hdr * uExposure);
    
    mapped = pow(mapped, 1.0 / vec3(gamma));

    out_FragColor = vec4(mapped, 1.0);
}
