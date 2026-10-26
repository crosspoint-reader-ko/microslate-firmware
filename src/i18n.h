#pragma once

#include <cstddef>
#include <cstdint>

// UI language. Value is persisted in NVS ("lang"); 0 = English keeps old behaviour.
enum class Language : uint8_t { ENGLISH = 0, KOREAN = 1 };
static constexpr int LANGUAGE_COUNT = 2;

// Every user-visible UI string: X(ID, English, Korean).
// "_FMT" entries are printf formats; both languages take the same arguments in
// the same order, so Korean wording must not reorder them.
#define I18N_STRINGS(X) \
  /* main menu */ \
  X(MENU_BROWSE,        "Browse Files",    "노트 목록") \
  X(MENU_NEW_NOTE,      "New Note",        "새 노트") \
  X(MENU_SETTINGS,      "Settings",        "설정") \
  X(MENU_SYNC,          "Sync",            "동기화") \
  X(HINT_MAIN,          "Arrows: Navigate  Enter: Select", "방향키: 이동  Enter: 선택") \
  X(BLE_KB_CONNECTED,   "KB Connected",    "키보드 연결됨") \
  X(BLE_KB_DISCONNECTED,"KB Disconnected", "키보드 연결 안 됨") \
  X(SCANNING,           "Scanning...",     "검색 중...") \
  X(CONNECTING,         "Connecting...",   "연결 중...") \
  /* file browser */ \
  X(TITLE_NOTES,        "Notes",           "노트") \
  X(NO_NOTES,           "No notes yet.",   "아직 노트가 없습니다.") \
  X(HINT_CREATE_NOTE,   "Press Ctrl+N to create one.", "Ctrl+N을 눌러 새 노트를 만드세요.") \
  X(HINT_BROWSER,       "Ctrl+N:Title  Ctrl+D:Delete", "Ctrl+N:새 노트  Ctrl+D:삭제") \
  X(DELETE_CONFIRM,     "Delete? Enter:Yes  Esc:No", "삭제할까요? Enter:예  Esc:아니오") \
  /* editor */ \
  X(WORD_ONE,           "1 word",          "1단어") \
  X(WORDS_FMT,          "%d words",        "%d단어") \
  X(PAGE_FMT,           "Pg %d/%d",        "%d/%d쪽") \
  X(FIND_FMT,           "Find: %s",        "찾기: %s") \
  X(HINT_FIND,          "Enter:Next Esc:Close", "Enter:다음 Esc:닫기") \
  /* title edit */ \
  X(TITLE_EDIT,         "Edit Title",      "제목 편집") \
  X(NOTE_TITLE_LABEL,   "Note title:",     "노트 제목:") \
  X(TITLE_QWERTY_NOTE,  "File title input uses US QWERTY", "제목은 US QWERTY로 입력합니다") \
  X(HINT_CONFIRM,       "Enter: Confirm   Esc: Cancel", "Enter: 확인   Esc: 취소") \
  /* settings */ \
  X(TITLE_SETTINGS,     "Settings",        "설정") \
  X(SET_ORIENTATION,    "Orientation",     "화면 방향") \
  X(SET_DARK_MODE,      "Dark Mode",       "다크 모드") \
  X(SET_WRITING_MODE,   "Writing Mode",    "쓰기 모드") \
  X(SET_FONT_SIZE,      "Font Size",       "글자 크기") \
  X(SET_KB_LAYOUT,      "Keyboard Layout", "키보드 배열") \
  X(SET_BLE_POWER,      "BLE Power",       "BLE 출력") \
  X(SET_BLUETOOTH,      "Bluetooth",       "블루투스") \
  X(SET_PAIRED,         "Paired Keyboards","등록된 키보드") \
  X(SET_LANGUAGE,       "Language",        "언어") \
  X(ORIENT_PORTRAIT,    "Portrait",        "세로") \
  X(ORIENT_LAND_CW,     "Landscape CW",    "가로 시계") \
  X(ORIENT_INVERTED,    "Inverted",        "세로 반전") \
  X(ORIENT_LAND_CCW,    "Landscape CCW",   "가로 반시계") \
  X(DARK,               "Dark",            "어두움") \
  X(LIGHT,              "Light",           "밝음") \
  X(MODE_NORMAL,        "Normal",          "일반") \
  X(MODE_TYPEWRITER,    "Typewriter",      "타자기") \
  X(MODE_PAGINATION,    "Pagination",      "페이지") \
  X(SIZE_SMALL,         "Small",           "작게") \
  X(SIZE_MEDIUM,        "Medium",          "보통") \
  X(SIZE_LARGE,         "Large",           "크게") \
  X(NONE,               "None",            "없음") \
  X(ONE_KEYBOARD,       "1 keyboard",      "키보드 1개") \
  X(KEYBOARDS_COUNT_FMT,"%d keyboards",    "키보드 %d개") \
  X(TX_BATTERY,         "Battery",         "절전") \
  X(TX_MAX,             "Max",             "최대") \
  X(HINT_SETTINGS,      "Arrows:Navigate  Enter:Change  Esc:Back", "방향키:이동  Enter:변경  Esc:뒤로") \
  /* bluetooth */ \
  X(TITLE_BT,           "Bluetooth Devices", "블루투스 기기") \
  X(BT_CONNECTED,       "Connected to keyboard", "키보드에 연결됨") \
  X(BT_SCANNING,        "Scanning for devices...", "기기 검색 중...") \
  X(BT_NOT_CONNECTED,   "Not connected",   "연결 안 됨") \
  X(PAIRED_FMT,         "Paired: %s",      "등록됨: %s") \
  X(PAIRING_CODE,       "PAIRING CODE:",   "페어링 코드:") \
  X(TYPE_CODE_1,        "Type this code on your keyboard", "키보드에서 이 코드를 입력하고") \
  X(TYPE_CODE_2,        "then press Enter", "Enter를 누르세요") \
  X(SEARCHING_FMT,      "Searching for devices%s", "기기 검색 중%s") \
  X(FOUND_FMT,          "Found: %d",       "발견: %d") \
  X(DEVICES_FMT,        "Devices: %d",     "기기: %d") \
  X(KEYBOARDS_FMT,      "Keyboards: %d",   "키보드: %d") \
  X(SHOW_KB_ONLY,       "Show keyboards only", "키보드만 보기") \
  X(SHOW_OTHER_FMT,     "Show other devices (%d)", "다른 기기 보기 (%d)") \
  X(PAGE_NUM_FMT,       "Page %d/%d",      "%d/%d쪽") \
  X(NO_DEVICES,         "No devices found","기기를 찾지 못했습니다") \
  X(HINT_SCAN_ENTER,    "Press Enter to scan for devices", "Enter를 눌러 기기를 검색하세요") \
  X(HINT_BT_1,          "Enter:Connect  Right:Scan", "Enter:연결  오른쪽:검색") \
  X(HINT_BT_2,          "Left:Disconnect  Esc:Back", "왼쪽:연결 해제  Esc:뒤로") \
  /* paired keyboards */ \
  X(TITLE_PAIRED,       "Paired Keyboards","등록된 키보드") \
  X(NO_PAIRED,          "No paired keyboards", "등록된 키보드가 없습니다") \
  X(HINT_GO_BT,         "Go to Bluetooth to scan and connect", "블루투스에서 검색해 연결하세요") \
  X(TAG_ACTIVE,         "active",          "사용 중") \
  X(TAG_LAST,           "last",            "최근") \
  X(FORGET_FMT,         "Forget \"%s\"?",  "\"%s\" 삭제할까요?") \
  X(HINT_YES_NO,        "Enter:Yes   Esc:No", "Enter:예   Esc:아니오") \
  X(HINT_PAIRED_1,      "Up/Down:Move   Enter:Connect", "위/아래:이동   Enter:연결") \
  X(HINT_PAIRED_2,      "Right:Forget   Esc:Back", "오른쪽:삭제   Esc:뒤로") \
  /* wifi sync */ \
  X(TITLE_SYNC,         "Sync",            "동기화") \
  X(SCANNING_NETWORKS,  "Scanning for networks...", "네트워크 검색 중...") \
  X(NO_NETWORKS,        "No networks found", "네트워크를 찾지 못했습니다") \
  X(SCAN_FAILED,        "Scan failed",     "검색 실패") \
  X(HINT_RESCAN,        "Enter: Rescan  Esc: Back", "Enter: 다시 검색  Esc: 뒤로") \
  X(SELECT_NETWORK,     "Select network:", "네트워크 선택:") \
  X(HINT_NETLIST,       "*=encrypted +=saved  Enter:Select  Esc:Back", "*=암호화 +=저장됨  Enter:선택  Esc:뒤로") \
  X(PASSWORD_FOR_FMT,   "Password for %s", "%s 비밀번호") \
  X(PASSWORD_QWERTY,    "Password input uses US QWERTY", "비밀번호는 US QWERTY로 입력합니다") \
  X(HINT_CONNECT,       "Enter: Connect   Esc: Cancel", "Enter: 연결   Esc: 취소") \
  X(HINT_CANCEL,        "Esc: Cancel",     "Esc: 취소") \
  X(CONNECTING_TO_FMT,  "Connecting to %s...", "연결 중: %s") \
  X(STAGE_WIFI,         "Connected to WiFi", "Wi-Fi 연결됨") \
  X(STAGE_PC,           "PC connected",    "PC 연결됨") \
  X(STAGE_FILE_FMT,     "File %d sent",    "파일 %d 전송됨") \
  X(STAGE_SYNC_DONE,    "Sync complete",   "동기화 완료") \
  X(SENT_CANCEL_FMT,    "Sent: %d   Esc: Cancel", "전송: %d   Esc: 취소") \
  X(SYNC_COMPLETE,      "Sync Complete",   "동기화 완료") \
  X(NO_CHANGES,         "No changes",      "변경 없음") \
  X(SENT_RECV_FMT,      "Sent: %d  Received: %d", "전송 %d  수신 %d") \
  X(RETURNING,          "Returning to menu...", "메뉴로 돌아갑니다...") \
  X(CONN_FAILED,        "Connection failed", "연결 실패") \
  X(HINT_RETRY,         "Enter: Retry   Esc: Back", "Enter: 재시도   Esc: 뒤로") \
  X(CONNECTED_BANG,     "Connected!",      "연결됨!") \
  X(SAVE_PASSWORD,      "Save password?",  "비밀번호를 저장할까요?") \
  X(HINT_YES_NO_ARROWS, "Enter/Up: Yes   Down/Esc: No", "Enter/위: 예   아래/Esc: 아니오") \
  X(SAVED_PW_FAILED,    "Saved password failed", "저장된 비밀번호로 연결 실패") \
  X(FORGET_PASSWORD,    "Forget saved password?", "저장된 비밀번호를 삭제할까요?") \
  /* sleep screen */ \
  X(ASLEEP,             "Asleep",          "절전 중") \
  X(HOLD_POWER,         "Hold Power to wake", "전원 버튼을 길게 눌러 깨우기")

enum class StrId : uint16_t {
#define I18N_ENUM(id, en, ko) id,
  I18N_STRINGS(I18N_ENUM)
#undef I18N_ENUM
  COUNT
};

Language getLanguage();
void setLanguage(Language lang);
Language languageFromIndex(int index);       // out-of-range -> ENGLISH
Language languageNext(Language lang);        // cycles
Language languagePrev(Language lang);
const char* languageName(Language lang);     // always in its own language: "English" / "한국어"

// String for the current language. Never returns nullptr. Static flash storage.
const char* tr(StrId id);
