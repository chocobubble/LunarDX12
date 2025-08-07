struct GeometryIn
{
    float4 posH : SV_POSITION;
    float2 size : TEXCOORD0;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD1;
    float rotation : TEXCOORD2;
    uint isActive : ACTIVE;
    uint blendMode : TEXCOORD3;
};

struct PixelIn 
{
    float4 posH : SV_POSITION;
    float4 color : COLOR;
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float size : TEXCOORD2;
    float rotation : TEXCOORD3;
    uint blendMode : TEXCOORD4;
};

cbuffer ParticleConstants : register(b1)
{
    float4x4 viewProjection;
    float3 cameraPosition;
    float deltaTime;
    float2 screenSize;
    float softParticleDistance;
    float padding;
};

[maxvertexcount(4)]
void main(point GeometryIn gIn[1], inout TriangleStream<PixelIn> stream)
{
    if (gIn[0].isActive == 0) {
        return; 
    }

    // 빌보드 벡터 계산 (카메라를 향하도록)
    float3 toCamera = normalize(cameraPosition - gIn[0].worldPos);
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, toCamera));
    up = cross(toCamera, right);
    
    // 파티클 크기
    float halfWidth = gIn[0].size.x * 0.5f;
    float halfHeight = gIn[0].size.y * 0.5f;
    
    // 회전 적용
    float cosRot = cos(gIn[0].rotation);
    float sinRot = sin(gIn[0].rotation);
    
    // 회전된 오프셋 계산
    float3 rightRotated = right * cosRot + up * sinRot;
    float3 upRotated = -right * sinRot + up * cosRot;
    
    PixelIn pIn;
    pIn.color = gIn[0].color;
    pIn.worldPos = gIn[0].worldPos;
    pIn.size = gIn[0].size.x;
    pIn.rotation = gIn[0].rotation;
    pIn.blendMode = gIn[0].blendMode;

    // 쿼드의 네 꼭짓점 생성
    float3 positions[4];
    positions[0] = gIn[0].worldPos + (-rightRotated - upRotated) * halfWidth; // 좌하
    positions[1] = gIn[0].worldPos + (rightRotated - upRotated) * halfWidth;  // 우하
    positions[2] = gIn[0].worldPos + (-rightRotated + upRotated) * halfWidth; // 좌상
    positions[3] = gIn[0].worldPos + (rightRotated + upRotated) * halfWidth;  // 우상
    
    float2 texCoords[4] = {
        float2(0.0, 1.0), // 좌하
        float2(1.0, 1.0), // 우하
        float2(0.0, 0.0), // 좌상
        float2(1.0, 0.0)  // 우상
    };
    
    // 삼각형 스트립으로 쿼드 생성
    for (int i = 0; i < 4; i++)
    {
        pIn.posH = mul(float4(positions[i], 1.0), viewProjection);
        pIn.texCoord = texCoords[i];
        stream.Append(pIn);
    }
    
    stream.RestartStrip();
}
