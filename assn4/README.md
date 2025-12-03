# Assignment 3-2: 3D Drawing with shader

<p align="right">20230232 반가운</p>
<p align="right">20230673 전재영</p>

OpenGL로 총알 피하기(Bullet Hell) 게임을 구현하는 과제입니다. 플레이어는 보스가 공격하는 총알을 피하고 공격해 적을 처치하면 승리합니다. 만약 플레이어가 총알에 맞아 모든 목숨이 사라지면 패배합니다.
이번 과제에서는 OpenGL Shader를 이용해 3D shader를 구현합니다.

---------

## 게임 플레이:
- 플레이어 조작: `W,A,S,D` 또는 `방향키`로 조작
- 공격: `SPACE`
- 재시작: `R`
- 종료: `ESC`
- 카메라 전환: `1`, `2`, `3`
- Polygon 모드 전환: `4`, `5`

## 실행 방법: 
1) Window

#### 파일 구조
- `Assignment1.sln`: 파일을 총괄하는 VS 프로젝트 파일입니다.
- `bin`: `freeglut.dll`, `glew32.dll`가 있는 라이브러리 폴더입니다.
- `include`: OpenGL의 헤더 파일들이 들어있는 폴더입니다.
- `lib`: `freeglut.lib`, `glew32.lib`가 있는 라이브러리 폴더입니다.
- `assets` : .obj 3D 모델링 파일이 들어있는 폴더입니다.

#### 빌드 방법
1) `freeglut`와 `glew32`를 설치합니다.

2) Visual Studio에서 솔루션 구성을 `Release`로 설정합니다.

3) 프로젝트 -> 속성 -> 디버깅에서 환경 변수를 설정합니다. 
여기서는 `PATH=..\bin;%PATH%`으로 설정했습니다.

4) 프로젝트 -> 속성 -> VC++ 디렉터리에서 포함 디렉터리에 `include`한 헤더 파일의 위치를 넣습니다. 
여기서는 `..\include`으로 설정했습니다.

5) 프로젝트 -> 속성 -> 링커 -> 입력 -> 추가 종속성에서 추가한 라이브러리의 위치를 넣습니다. 여기서는 `..\lib\freeglut.lib` `..\lib\glew32.lib`로 설정했습니다.

6) 이후, `Release` 모드로 `main.cpp` 파일을 실행하면 됩니다.