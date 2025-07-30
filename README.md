## 개요

DirectX 12를 기반으로 구축한 3D 렌더링 엔진입니다. 현대적인 그래픽스 기법들을 단계적으로 구현하며, 각 기능의 개발 과정과 문제 해결 과정을 문서화했습니다.

## 주요 구현 기능

- **PBR (Physically Based Rendering)** - Cook-Torrance BRDF 모델
- **IBL (Image Based Lighting)** - HDR 환경 맵 기반 조명
- **테셀레이션** - Hull/Domain Shader를 활용한 적응적 지오메트리 세분화
- **그림자 매핑** - 깊이 버퍼 기반 실시간 그림자
- **노말 매핑** - TBN 공간 변환을 통한 디테일 향상
- **GPU 기반 파티클 시스템** - Compute Shader 활용
- **거울 반사** - Stencil Buffer를 활용한 평면 반사
- **빌보드 렌더링** - Geometry Shader 기반
- **포스트 프로세싱** - Compute Shader 활용한 가우시안 블러

## 상세 문서

### **[프로젝트 위키 바로가기](../../wiki)**

## 주요 결과물

| 기능 | 구현 내용 | 개발일지 |
|------|-----------|----------|
| PBR 렌더링 | Cook-Torrance 모델, Metallic-Roughness 워크플로우 | [상세보기](../../wiki/2025-07-06-PBR) |
| IBL 시스템 | Irradiance Map, Prefiltered Environment Map | [상세보기](../../wiki/2025-07-12-Ibl) |
| 하드웨어 테셀레이션 | 거리 기반 적응적 지오메트리 세분화, 와이어프레임 시각화 | [상세보기](../../wiki/2025-07-09-Tessellation) |
| 파티클 시스템 | GPU 기반 물리 시뮬레이션 | [상세보기](../../wiki/2025-07-05-Particle) |

## 학습 자료

이 프로젝트는 다음 자료들을 참고하여 개발되었습니다:

- [홍정모 연구소 컴퓨터 그래픽스 새싹코스](https://www.honglab.ai/courses/graphicspt1)
- DirectX 12를 이용한 3D 게임 프로그래밍 입문 (Frank Luna)

## License

This project is for portfolio demonstration purposes only. All rights reserved.
