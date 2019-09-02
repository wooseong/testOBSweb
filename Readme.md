# 소마 10기 //TODO팀
프로젝트  : SCADE

####문제정의
 - 문화체육관광부가 발표한 ‘2016 콘텐츠산업 통계조사’에 따르면 지난 2015년도 기준 국내 콘텐츠산업의 매출액은 전년 대비 5.8% 증가한 100조 4863억원으로 집계됐다. 이중 1인 방송과 긴밀하게 관련된 방송 부문의 매출은 16조 4630억원에 달한다.
  이런 추세에 따라 여러 진흥원과 재단 등에서 1인 미디어 제작을 위한 교육, 촬영실, 공간 등을 지원하지만 스트리머만을 위한 소프트웨어 프로그램은 지원은 부족하다.
   실제 “2019 서울 1인방송 미디어쇼”에 참가한 68개 업체 중 콘텐츠를 제작하는데 사용되는 소프트웨어 프로그램에 관련된 업체는 한 곳도 없다는 것을 알 수 있다.


####기획의도
 - 스트리머라는 새로운 직업에 맞춘 프로그램 부족을 해결하고 기존에 존재하던 스트리밍, 영상 편집 프로그램을 그대로 가져와서 사용하는 것이 아닌 스트리머에 맞춰 변형한 프로그램을 제공한다.

####프로젝트 소개
 - (Streaming/파일편집) 스트리머 전용 스트리밍, 영상 편집 프로그램이다.
 - (환경 Easy Setup) 방송의 송출 환경 설정을 쉽게 한다.
 - (Custom Control Pannel) 2개의 컴퓨터 스위치, 직관적인 화면 구성 등을 도와 방송 중 시청자와 인터액션을 더 간편하게 할 수 있게 한다.(OBS Remote Control 연계 / 2개이상 시스템 컨트롤)
 - (Meta & 파일 분리 저장) 방송에 송출된 화면, 웹캠, 오디오, 채팅, 도네이션, 시청자 정보 등의 정보를 따로 저장하여 사용할 수 있다.
 - (편집 어시스트-하일라이팅) 영상 내에 편집될 구간을 선별해주어 편집시간을 단축할 수 있다.
 - (자원 재활용) 송출 후 하이라이트 영상을 만들기 쉽도록 반복되는 작업들을 자동화한다.

![구조도](/assets/구조도.png)

####적용 기술
- OBS
>What is OBS Studio?
OBS Studio is software designed for capturing, compositing, encoding, recording, and streaming video content, efficiently.

>Quick Links
Website: https://obsproject.com
https://github.com/obsproject/obs-studio.git

 - OpenShot
 >What is OpenShot?
 OpenShot Video Library (libopenshot) is a free, open-source C++ library dedicated to delivering high quality video editing, animation, and playback solutions to the world.

 >Quick Links
 Website: https://www.openshot.org/
 https://github.com/OpenShot/libopenshot.git
