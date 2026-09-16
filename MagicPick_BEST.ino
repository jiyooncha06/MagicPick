#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

// ======================================================
// 매직픽 V3.2
//
// 기능
// 1. SH1106 OLED 한글 출력
// 2. Y축 기울기로 모드 변경
// 3. 흔들어서 랜덤 추천
// 4. 흔드는 동안 모드 변경 차단
// 5. 미션 / 먹거리 / 놀이기구 추천
// 6. 하단에 짧은 영어 MAGIC HINT 출력
// 7. 결과 화면에서 다시 흔들면 재추첨
// 8. 8초 뒤 메뉴 자동 복귀
// ======================================================


// ======================================================
// 장치 설정
// ======================================================

const int SDA_PIN = D4;
const int SCL_PIN = D5;

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

Adafruit_MPU6050 mpu;


// ======================================================
// 모드 및 화면 상태
// ======================================================

enum MagicMode {
  MODE_MISSION = 0,
  MODE_FOOD = 1,
  MODE_RIDE = 2
};

enum ScreenState {
  SCREEN_MENU,
  SCREEN_RESULT
};

MagicMode currentMode = MODE_FOOD;
ScreenState screenState = SCREEN_MENU;


// ======================================================
// 기울기 설정
// ======================================================

const float TILT_TRIGGER = 4.5;
const float CENTER_THRESHOLD = 2.0;

bool tiltLocked = false;


// ======================================================
// 흔들기 설정
// ======================================================

// 낮추면 흔들기에 더 민감해짐
// 높이면 더 세게 흔들어야 함
const float SHAKE_THRESHOLD = 6.5;

// 이 정도 이상의 빠른 움직임에서는
// 기울기 모드 변경을 차단
const float MOTION_BLOCK_THRESHOLD = 2.5;

// 회전 속도가 빠를 때도 모드 변경 차단
const float GYRO_BLOCK_THRESHOLD = 1.3;

// 흔들기 한 번이 여러 번 인식되는 것을 방지
const unsigned long SHAKE_COOLDOWN = 1200;

unsigned long lastShakeTime = 0;


// ======================================================
// 결과 화면 설정
// ======================================================

const unsigned long RESULT_DURATION = 8000;
unsigned long resultShownTime = 0;


// ======================================================
// 추천 데이터 구조
// ======================================================

struct Recommendation {
  const char* line1;
  const char* line2;
};


// ======================================================
// 미션 목록
// YOU & I 주제에 맞춘 2인 이상 미션
// ======================================================

const Recommendation missions[] = {
  {"성 앞에서", "같은 포즈 찍기"},
  {"서로의 사진", "한 장씩 찍기"},
  {"둘이 동시에", "손하트 만들기"},
  {"서로 어울리는", "머리띠 골라주기"},
  {"로티나 로리와", "함께 사진 찍기"},
  {"같은 색 소품", "찾아서 찍기"},
  {"퍼레이드에서", "함께 손 흔들기"},
  {"서로의 최애", "놀이기구 맞히기"},
  {"간식 하나를", "함께 나눠 먹기"},
  {"탑승 전 둘만의", "구호 만들기"},
  {"베스트 포즈를", "서로 정해주기"},
  {"어울리는 캐릭터", "서로 골라주기"},
  {"서로의 사진", "세 장씩 찍기"},
  {"같은 표정으로", "사진 찍기"},
  {"기대되는 순간", "하나씩 말하기"},
  {"포토존을 골라", "함께 사진 찍기"},
  {"서로에게 오늘의", "별명 지어주기"},
  {"탑승한 뒤", "한 줄 평 말하기"},
  {"오늘의 베스트 컷", "함께 고르기"},
  {"가장 좋았던 순간", "하나씩 말하기"}
};

const int MISSION_COUNT =
  sizeof(missions) / sizeof(missions[0]);


// ======================================================
// 먹거리 목록
// ======================================================

const Recommendation foods[] = {
  {"로티로리", "치즈빵"},
  {"자이로", "츄러스"},
  {"콜팝", ""},
  {"일반", "소떡소떡"},
  {"슈프림", "소떡소떡"},
  {"솜사탕", ""},
  {"아이스 딸기", "탕후루"},
  {"팝콘", ""},
  {"소금빵", "젤라또"},
  {"츄러스", ""},
  {"오레오", "츄러스"},
  {"타코야끼", ""},
  {"델리본", "감자튀김"},
  {"로리", "슬러시"},
  {"아이스", "팩토리"},
  {"뽀득", "소세지"},
  {"빙수", "슬러시"},
  {"치즈감자", ""},
  {"로티와플", ""},
  {"로티만쥬", ""},
  {"프레페레", ""},
  {"칠리치즈", "감자"},
  {"마약", "옥수수"},
  {"하트", "츄러스"},
  {"치킨", "아이스크림"},
  {"델리치즈", "포테이토"},
  {"뻥치콜", ""},
  {"새우꼬치", ""},
  {"닭꼬치", ""},
  {"TGI", "스테이크"},
  {"원킬치킨", ""},
  {"원킬", "소시지"},
  {"델키스", "케밥"}
};

const int FOOD_COUNT =
  sizeof(foods) / sizeof(foods[0]);


// ======================================================
// 놀이기구 목록
// ======================================================

const Recommendation rides[] = {
  {"귀담", ""},
  {"마도신당", ""},
  {"아르키나", "라이드"},
  {"에오스", "타워"},
  {"스톤", "익스프레스"},
  {"배틀그라운드", "월드 에이전트"},
  {"카트라이더", "레이싱월드"},
  {"월드", "모노레일"},
  {"자이로드롭", ""},
  {"자이로스윙", ""},
  {"자이로스핀", ""},
  {"아틀란티스", ""},
  {"혜성특급", ""},
  {"스페인", "해적선"},
  {"신밧드의", "모험"},
  {"회전목마", ""},
  {"후룸라이드", ""},
  {"회전바구니", ""},
  {"드래곤", "와일드슈팅"},
  {"플라이벤처", ""},
  {"후렌치", "레볼루션"},
  {"범퍼카", ""},
  {"파라오의", "분노"},
  {"풍선비행", ""},
  {"로티트레인", ""}
};

const int RIDE_COUNT =
  sizeof(rides) / sizeof(rides[0]);


// ======================================================
// 영어 MAGIC HINT
//
// 작은 영어 글꼴에서 한 줄에 들어갈 수 있도록
// 짧은 문구로 구성
// ======================================================

// 미션용
const char* missionHints[] = {
  "DO IT TOGETHER",
  "MAKE A MEMORY",
  "MATCH YOUR POSE",
  "SMILE TOGETHER",
  "SHARE THE MAGIC",
  "KEEP THIS MOMENT",
  "FOLLOW THE RED",
  "STAY TOGETHER"
};

const int MISSION_HINT_COUNT =
  sizeof(missionHints) / sizeof(missionHints[0]);


// 먹거리용
const char* foodHints[] = {
  "SHARE A BITE",
  "TASTE THE MAGIC",
  "PICK THE SWEET ONE",
  "SWEET LUCK AWAITS",
  "TRY SOMETHING NEW",
  "ONE BITE OF LUCK",
  "FOLLOW THE RED",
  "SHARE THE SWEETNESS"
};

const int FOOD_HINT_COUNT =
  sizeof(foodHints) / sizeof(foodHints[0]);


// 놀이기구용
const char* rideHints[] = {
  "TAKE THE CHANCE",
  "TRUST YOUR PICK",
  "CHASE THE THRILL",
  "ADVENTURE AWAITS",
  "BE BRAVE TODAY",
  "RIDE THE MAGIC",
  "FOLLOW THE LIGHT",
  "LUCK IS NEAR"
};

const int RIDE_HINT_COUNT =
  sizeof(rideHints) / sizeof(rideHints[0]);


// ======================================================
// 직전 결과 중복 방지
// ======================================================

int previousMissionIndex = -1;
int previousFoodIndex = -1;
int previousRideIndex = -1;


// ======================================================
// 화면 출력 보조 함수
// ======================================================

// 한글 UTF-8 문자열 중앙 정렬
void drawCenteredUTF8(int y, const char* text) {
  int textWidth = display.getUTF8Width(text);
  int x = (128 - textWidth) / 2;

  if (x < 0) {
    x = 0;
  }

  display.drawUTF8(x, y, text);
}


// 영문 문자열 중앙 정렬
void drawCenteredEnglish(int y, const char* text) {
  int textWidth = display.getStrWidth(text);
  int x = (128 - textWidth) / 2;

  if (x < 0) {
    x = 0;
  }

  display.drawStr(x, y, text);
}


// ======================================================
// 현재 모드 이름
// ======================================================

const char* getModeName(MagicMode mode) {
  switch (mode) {
    case MODE_MISSION:
      return "미션";

    case MODE_FOOD:
      return "먹거리";

    case MODE_RIDE:
      return "놀이기구";
  }

  return "먹거리";
}


// ======================================================
// 결과 화면 상단 영문 제목
// ======================================================

const char* getResultTitle(MagicMode mode) {
  switch (mode) {
    case MODE_MISSION:
      return "MISSION PICK";

    case MODE_FOOD:
      return "FOOD PICK";

    case MODE_RIDE:
      return "RIDE PICK";
  }

  return "MAGIC PICK";
}


// ======================================================
// 시작 화면
// ======================================================

void drawSplashScreen() {
  display.clearBuffer();

  display.setFont(u8g2_font_unifont_t_korean2);

  drawCenteredUTF8(25, "매직픽");
  drawCenteredUTF8(49, "기울여 선택");

  display.sendBuffer();
}


// ======================================================
// 메뉴 화면
// ======================================================

void drawMenu() {
  screenState = SCREEN_MENU;

  display.clearBuffer();

  display.drawFrame(2, 1, 124, 62);

  // 상단 미니 장식
  display.drawDisc(15, 12, 2);
  display.drawDisc(113, 12, 2);
  display.drawLine(21, 12, 45, 12);
  display.drawLine(83, 12, 107, 12);

  display.setFont(u8g2_font_unifont_t_korean2);

  drawCenteredUTF8(35, getModeName(currentMode));
  drawCenteredUTF8(58, "기울여 변경");

  // 좌우 화살표
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(10, 56, "<");
  display.drawStr(112, 56, ">");

  display.sendBuffer();
}


// ======================================================
// 추첨 중 화면
// ======================================================

void drawSelectingScreen() {
  display.clearBuffer();

  // 상단 영문
  display.setFont(u8g2_font_6x12_tf);
  drawCenteredEnglish(13, "MAGIC PICK");

  display.drawLine(22, 18, 106, 18);

  // 중앙 한글
  display.setFont(u8g2_font_unifont_t_korean2);
  drawCenteredUTF8(40, "두근두근");
  drawCenteredUTF8(60, "결과를 고르는 중");

  display.sendBuffer();

  delay(450);
}


// ======================================================
// 카테고리별 영어 힌트 선택
// ======================================================

const char* getRandomMagicHint(MagicMode mode) {
  switch (mode) {
    case MODE_MISSION:
      return missionHints[random(MISSION_HINT_COUNT)];

    case MODE_FOOD:
      return foodHints[random(FOOD_HINT_COUNT)];

    case MODE_RIDE:
      return rideHints[random(RIDE_HINT_COUNT)];
  }

  return "TRUST YOUR PICK";
}


// ======================================================
// 결과 화면
// ======================================================

void drawResult(
  MagicMode selectedMode,
  const Recommendation& result,
  const char* magicHint
) {
  screenState = SCREEN_RESULT;
  resultShownTime = millis();

  display.clearBuffer();

  // --------------------------------------------------
  // 상단 카테고리 제목
  // --------------------------------------------------

  display.setFont(u8g2_font_6x12_tf);

  drawCenteredEnglish(
    10,
    getResultTitle(selectedMode)
  );

  display.drawLine(14, 14, 114, 14);


  // --------------------------------------------------
  // 중앙 추천 결과
  // --------------------------------------------------

  display.setFont(u8g2_font_unifont_t_korean2);

  if (result.line2[0] == '\0') {
    // 결과가 한 줄인 경우
    drawCenteredUTF8(39, result.line1);
  } else {
    // 결과가 두 줄인 경우
    drawCenteredUTF8(31, result.line1);
    drawCenteredUTF8(47, result.line2);
  }


  // --------------------------------------------------
  // 하단 영어 MAGIC HINT
  // --------------------------------------------------

  display.setFont(u8g2_font_5x8_tf);

  drawCenteredEnglish(62, magicHint);

  display.sendBuffer();
}


// ======================================================
// 직전 결과와 다른 랜덤 인덱스 생성
// ======================================================

int getNewRandomIndex(int count, int previousIndex) {
  if (count <= 1) {
    return 0;
  }

  int newIndex;

  do {
    newIndex = random(count);
  } while (newIndex == previousIndex);

  return newIndex;
}


// ======================================================
// 현재 선택한 모드에서 랜덤 결과 추첨
// ======================================================

void showRandomResult(MagicMode selectedMode) {
  drawSelectingScreen();

  const char* magicHint =
    getRandomMagicHint(selectedMode);

  switch (selectedMode) {
    case MODE_MISSION: {
      int index = getNewRandomIndex(
        MISSION_COUNT,
        previousMissionIndex
      );

      previousMissionIndex = index;

      drawResult(
        selectedMode,
        missions[index],
        magicHint
      );

      Serial.print("미션 결과: ");
      Serial.print(missions[index].line1);
      Serial.print(" ");
      Serial.print(missions[index].line2);
      Serial.print(" | ");
      Serial.println(magicHint);

      break;
    }

    case MODE_FOOD: {
      int index = getNewRandomIndex(
        FOOD_COUNT,
        previousFoodIndex
      );

      previousFoodIndex = index;

      drawResult(
        selectedMode,
        foods[index],
        magicHint
      );

      Serial.print("먹거리 결과: ");
      Serial.print(foods[index].line1);
      Serial.print(" ");
      Serial.print(foods[index].line2);
      Serial.print(" | ");
      Serial.println(magicHint);

      break;
    }

    case MODE_RIDE: {
      int index = getNewRandomIndex(
        RIDE_COUNT,
        previousRideIndex
      );

      previousRideIndex = index;

      drawResult(
        selectedMode,
        rides[index],
        magicHint
      );

      Serial.print("놀이기구 결과: ");
      Serial.print(rides[index].line1);
      Serial.print(" ");
      Serial.print(rides[index].line2);
      Serial.print(" | ");
      Serial.println(magicHint);

      break;
    }
  }
}


// ======================================================
// 오른쪽으로 모드 이동
// ======================================================

void moveModeRight() {
  int nextMode = static_cast<int>(currentMode) + 1;

  if (nextMode > MODE_RIDE) {
    nextMode = MODE_MISSION;
  }

  currentMode = static_cast<MagicMode>(nextMode);

  drawMenu();

  Serial.print("오른쪽 이동 → ");
  Serial.println(getModeName(currentMode));
}


// ======================================================
// 왼쪽으로 모드 이동
// ======================================================

void moveModeLeft() {
  int previousMode = static_cast<int>(currentMode) - 1;

  if (previousMode < MODE_MISSION) {
    previousMode = MODE_RIDE;
  }

  currentMode = static_cast<MagicMode>(previousMode);

  drawMenu();

  Serial.print("왼쪽 이동 → ");
  Serial.println(getModeName(currentMode));
}


// ======================================================
// 기울기 입력 처리
// ======================================================

void updateModeFromTilt(float tiltY) {
  if (screenState != SCREEN_MENU) {
    return;
  }

  // 중앙으로 되돌아오면 잠금 해제
  if (
    tiltLocked &&
    fabs(tiltY) < CENTER_THRESHOLD
  ) {
    tiltLocked = false;
    return;
  }

  if (tiltLocked) {
    return;
  }

  if (tiltY <= -TILT_TRIGGER) {
    tiltLocked = true;
    moveModeLeft();
    return;
  }

  if (tiltY >= TILT_TRIGGER) {
    tiltLocked = true;
    moveModeRight();
    return;
  }
}


// ======================================================
// 흔들기 강도 계산
// ======================================================

float calculateShakeStrength(
  float accelX,
  float accelY,
  float accelZ
) {
  float totalAcceleration = sqrt(
    accelX * accelX +
    accelY * accelY +
    accelZ * accelZ
  );

  return fabs(totalAcceleration - 9.81);
}


// ======================================================
// 회전 속도 계산
// ======================================================

float calculateGyroStrength(
  float gyroX,
  float gyroY,
  float gyroZ
) {
  return sqrt(
    gyroX * gyroX +
    gyroY * gyroY +
    gyroZ * gyroZ
  );
}


// ======================================================
// 흔들기 감지
// ======================================================

bool detectShake(float shakeStrength) {
  if (
    shakeStrength >= SHAKE_THRESHOLD &&
    millis() - lastShakeTime >= SHAKE_COOLDOWN
  ) {
    lastShakeTime = millis();

    Serial.print("흔들기 감지! 강도: ");
    Serial.println(shakeStrength, 2);

    return true;
  }

  return false;
}


// ======================================================
// MPU6050 전체 처리
// ======================================================

void updateMotion() {
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temperature;

  mpu.getEvent(&accel, &gyro, &temperature);

  float accelX = accel.acceleration.x;
  float accelY = accel.acceleration.y;
  float accelZ = accel.acceleration.z;

  float gyroX = gyro.gyro.x;
  float gyroY = gyro.gyro.y;
  float gyroZ = gyro.gyro.z;

  float shakeStrength = calculateShakeStrength(
    accelX,
    accelY,
    accelZ
  );

  float gyroStrength = calculateGyroStrength(
    gyroX,
    gyroY,
    gyroZ
  );


  // --------------------------------------------------
  // 흔들기 감지를 먼저 처리
  //
  // 흔드는 과정에서 Y축 값이 바뀌더라도
  // 선택된 모드가 변경되지 않도록 함
  // --------------------------------------------------

  if (detectShake(shakeStrength)) {
    MagicMode selectedMode = currentMode;

    tiltLocked = true;

    showRandomResult(selectedMode);
    return;
  }


  // 결과 화면에서는 기울기 변경 금지
  if (screenState != SCREEN_MENU) {
    return;
  }


  // 빠르게 움직이는 동안에는 모드 변경 차단
  bool isMovingFast =
    shakeStrength >= MOTION_BLOCK_THRESHOLD ||
    gyroStrength >= GYRO_BLOCK_THRESHOLD;

  if (isMovingFast) {
    return;
  }


  // 안정적인 움직임일 때만 기울기 입력 처리
  updateModeFromTilt(accelY);
}


// ======================================================
// 결과 화면 자동 복귀
// ======================================================

void updateResultTimeout() {
  if (
    screenState == SCREEN_RESULT &&
    millis() - resultShownTime >= RESULT_DURATION
  ) {
    // 메뉴로 돌아온 직후 오작동 방지
    tiltLocked = true;

    drawMenu();

    Serial.println("결과 표시 종료 → 메뉴 복귀");
  }
}


// ======================================================
// 초기 설정
// ======================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(SDA_PIN, SCL_PIN);

  display.begin();

  drawSplashScreen();
  delay(1200);

  if (!mpu.begin(0x68, &Wire)) {
    display.clearBuffer();

    display.setFont(u8g2_font_unifont_t_korean2);

    drawCenteredUTF8(28, "센서 연결 오류");
    drawCenteredUTF8(50, "배선 확인");

    display.sendBuffer();

    Serial.println("MPU6050 연결 실패");

    while (true) {
      delay(100);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // 매번 같은 순서로 결과가 나오지 않도록 난수 초기화
  randomSeed(
    micros() +
    static_cast<unsigned long>(
      analogRead(A0)
    )
  );

  Serial.println("매직픽 V3.2 시작");
  Serial.println("기울여 모드를 고르고 흔들어 주세요.");

  drawMenu();
}


// ======================================================
// 반복 실행
// ======================================================

void loop() {
  updateMotion();
  updateResultTimeout();

  delay(30);
}
