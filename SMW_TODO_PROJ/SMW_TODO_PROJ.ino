/*  SOMA_방송 분배기 ver 1.2
    //TODO

    ver 1.0 프레임 개발 및 회로도 작업 후 릴레이 제어
    ver 1.1 구동 코드 함수화
    ver 1.2 매크로 기능 구현
*/
#include <Keyboard.h>

// 핀배열
#define com1 2 //btn red
#define com2 3  //btn green
#define comout1 4 //릴레이 in1
#define comout2 5 //릴레이 in2
#define macro1 6 //메크로 in1
#define macro2 7 //메크로 in2
#define macro3 8 //메크로 in3


byte com1_state; //com1 상태 전역변수
byte com2_state; //com2 상태 전역변수
byte macro1_state; //macro1 상태 전역변수
byte macro2_state; //macro2 상태 전역변수
byte macro3_state; //macro3 상태 전역변수


void setup() {
  Serial.begin(9600); //시리얼 모니터 셋업
  Keyboard.begin();
  pinMode(com1, INPUT_PULLUP); //핀모드 셋업
  pinMode(com2, INPUT_PULLUP);
  pinMode(comout1, OUTPUT);
  pinMode(comout2, OUTPUT);
  pinMode(macro1, INPUT_PULLUP);
  pinMode(macro2, INPUT_PULLUP);
  pinMode(macro3, INPUT_PULLUP);
}

void State_refresh() //상태 갱신
{
  com1_state = digitalRead(com1);
  com2_state = digitalRead(com2);
  macro1_state = digitalRead(macro1);
  macro2_state = digitalRead(macro2);
  macro3_state = digitalRead(macro3);
}

void State_print() //상태 출력
{
  Serial.print("com1 state : ");
  Serial.print(com1_state);
  Serial.print(" com2 state : ");
  Serial.print(com2_state);
  Serial.print(" macro1 state : ");
  Serial.print(macro1_state);
  Serial.print(" macro2 state : ");
  Serial.print(macro2_state);
  Serial.print(" macro3 state : ");
  Serial.print(macro3_state);
  Serial.println("");
}

void Relay_control() //릴레이 컨트롤
{
  if (com1_state == LOW)
  {
    digitalWrite(comout1, HIGH);
    digitalWrite(comout2, HIGH);
  }
  else
  {
    digitalWrite(comout1, LOW);
    digitalWrite(comout2, LOW);
  }
}

void macro() {  //매크로 버튼 인식
  if (macro1_state == LOW)
  {
    Keyboard.press(''); //프로그램과 연동하면서 프로그램에 넣어줄 값과 동기화 해야함.
  }
  else
  {
    Keyboard.release('');
  }
  if (macro2_state == LOW)
  {
    Keyboard.press('');
  }
  else
  {
    Keyboard.release('');
  }
  if (macro3_state == LOW)
  {
    Keyboard.press('');
  }
  else
  {
    Keyboard.release('');
  }
}


void loop() {
  State_refresh();
  Relay_control();
  Statea_print();
  //macro(); //허브 부분 회로도 작업이 되지 않았기 때문에 이대로 올리면 PULLUP 관련해서 컴퓨터에 키보드가 무한으로 입력되기 때문에 회로도 완성후 업로드 해야 합니다.
}
