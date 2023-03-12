// scalepx: scale3x algorithm with single shader (color mixed), p stands for 'point'
/* algorithm source: https://web.archive.org/web/20160527015550/https://libretro.com/forums/archive/index.php?t-1655.html
 1=E; 2=E; 3=E; 4=E; 5=E; 6=E; 7=E; 8=E; 9=E;
 IF D==B AND D!=H AND B!=F => 1=D
 IF (D==B AND D!=H AND B!=F AND E!=C) OR (B==F AND B!=D AND F!=H AND E!=A) => 2=B
 IF B==F AND B!=D AND F!=H => 3=F
 IF (H==D AND H!=F AND D!=B AND E!=A) OR (D==B AND D!=H AND B!=F AND E!=G) => 4=D
 5=E
 IF (B==F AND B!=D AND F!=H AND E!=I) OR (F==H AND F!=B AND H!=D AND E!=C) => 6=F
 IF H==D AND H!=F AND D!=B => 7=D
 IF (F==H AND F!=B AND H!=D AND E!=G) OR (H==D AND H!=F AND D!=B AND E!=I) => 8=H
 IF F==H AND F!=B AND H!=D => 9=F
*/
#version 450

layout(location = 0) in vec2 tc;

layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(std140, push_constant) uniform ui{
    ivec2 vpsize; // viewport size in pixels
    float THRESHOLD;
};

//const float THRESHOLD = 0.5;

bool same(vec4 a, vec4 b){
    vec4 dv = a - b;
    return dot(dv,dv) < THRESHOLD;
}

vec4 bLerp(vec4 a, vec4 b, vec4 c, vec4 d, vec2 st){
    vec4 x = mix(a,b,st.t);
    vec4 y = mix(a,b,st.t);
    return mix(x,y,st.s);
}

void main() {
    const ivec2 TXSIZE = textureSize(tex,0);
    if(vpsize.x <= TXSIZE.x || vpsize.y <= TXSIZE.y){
        outColor = texture(tex, tc);
    }
    else{
        const vec2 SAMPLE_STRIDE = vec2(1.0) / vec2(TXSIZE);

        // 원본 텍셀 기준 주변 3x3 샘플
        const vec4 A = texture(tex, tc + vec2(-SAMPLE_STRIDE.x,-SAMPLE_STRIDE.y));
        const vec4 B = texture(tex, tc + vec2(0,-SAMPLE_STRIDE.y));
        const vec4 C = texture(tex, tc + vec2(SAMPLE_STRIDE.x,-SAMPLE_STRIDE.y));
        const vec4 D = texture(tex, tc + vec2(-SAMPLE_STRIDE.x,0));
        const vec4 E = texture(tex, tc + vec2(0,0));
        const vec4 F = texture(tex, tc + vec2(SAMPLE_STRIDE.x,0));
        const vec4 G = texture(tex, tc + vec2(-SAMPLE_STRIDE.x,SAMPLE_STRIDE.y));
        const vec4 H = texture(tex, tc + vec2(0,SAMPLE_STRIDE.y));
        const vec4 I = texture(tex, tc + vec2(SAMPLE_STRIDE.x,SAMPLE_STRIDE.y));

        const bool BD = same(B,D);
        const bool DH = same(D,H);
        const bool BF = same(B,F);
        const bool CE = same(C,E);
        const bool FH = same(F,H);
        const bool AE = same(A,E);
        const bool EG = same(E,G);
        const bool EI = same(E,I);

        const bool BH = same(B,H);
        const bool DF = same(D,F);

        bool D1 = false;
        bool B2 = false;
        bool F3 = false;
        bool D4 = false;
        bool F6 = false;
        bool D7 = false;
        bool H8 = false;
        bool F9 = false;

        if(!BH && !DF){
            D1 = BD;
            B2 = (BD && !CE) || (BF && !AE);
            F3 = BF;
            D4 = (BD && !EG) || (DH && !AE);
            F6 = (BF && !EI) || (FH && !CE);
            D7 = DH;
            H8 = (DH && !EI) || (FH && !EG);
            F9 = FH;
        }

        const vec4 COLOR33[3][3] = {
            {
                mix(E,D,float(D1)), // left up
                mix(E,B,float(B2)), // up
                mix(E,F,float(F3))  // right up
            },
            {
                mix(E,D,float(D4)), // left
                E,
                mix(E,F,float(F6))  // right
            },
            {
                mix(E,D,float(D7)), // left down
                mix(E,H,float(H8)), // down
                mix(E,F,float(F9))  // right down
            }
        };
        
        const vec2 IN_BOARD = mod(tc, SAMPLE_STRIDE) * vec2(TXSIZE); // [0,1]^2
        
        // 1. 4 adjacent
        vec2 leftUp = max((IN_BOARD - vec2(1.0/6.0)) * 3.0,0.0);
        vec2 rightDown = min((IN_BOARD + vec2(1.0/6.0))*3.0,2.0);

        // 2. blinear interpolation
        /*
        if(IN_BOARD.x < 1.0/3.0 && IN_BOARD.y < 1.0/3.0){
            outColor = COLOR33[0][0];
        }
        else if(IN_BOARD.x < 1.0/3.0 && IN_BOARD.y < 2.0/3.0){
            outColor = COLOR33[1][0];
        }
        else if(IN_BOARD.x < 1.0/3.0){
            outColor = COLOR33[2][0];
        }
        else if(IN_BOARD.x < 2.0/3.0 && IN_BOARD.y < 1.0/3.0){
            outColor = COLOR33[0][1];
        }
        else if(IN_BOARD.x < 2.0/3.0 && IN_BOARD.y < 2.0/3.0){
            outColor = COLOR33[1][1];
        }
        else if(IN_BOARD.x < 2.0/3.0){
            outColor = COLOR33[2][1];
        }
        else if(IN_BOARD.y < 1.0/3.0){
            outColor = COLOR33[0][2];
        }
        else if(IN_BOARD.y < 2.0/3.0){
            outColor = COLOR33[1][2];
        }
        else {
            outColor = COLOR33[2][2];
        }
*/
        //outColor = COLOR33[int(leftUp.y)][int(leftUp.x)];
        const vec2 ST = mod(IN_BOARD + vec2(1.0/6.0), vec2(1.0/3.0)) * 3.0;

        outColor = bLerp(
            COLOR33[int(leftUp.y)][int(leftUp.x)],
            COLOR33[int(rightDown.y)][int(leftUp.x)],
            COLOR33[int(leftUp.y)][int(rightDown.x)],
            COLOR33[int(rightDown.y)][int(rightDown.x)],
            ST
        );
    }
}