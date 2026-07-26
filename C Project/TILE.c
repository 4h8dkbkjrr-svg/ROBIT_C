//  ROBIT C PROJECT
//
//  TILE
//  ㄴ TILE.c
//
//  Created by Lee DY on 7/25/26.
//

// 중요 사항: 본 프로그램은 Mac의 터미널 또는 VisualStudioCode에서 구동해야합니다.

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>    // printf, getchar, 파일 입출력, setvbuf
#include <string.h>   // strncpy, strcpy (이름 문자열을 다루기 위해 필요)
#include <stdlib.h>   // rand, srand (정답 타일을 무작위로 고르기 위해 필요)
#include <time.h>     // time (난수 씨드), nanosleep (시간 지연)

#include <termios.h>  // struct termios, tcgetattr, tcsetattr (터미널 입력 방식을 제어)

#define TILE_STDIN 0 // 표준 입력의 파일 번호. 0/1/2 는 커널이 정한 약속이고,
                     // tcgetattr 등은 int 하나를 받으므로 직접 정의해 쓴다

#define SIZE 3 // 시작할 때의 판 크기
#define MAX_SIZE 5 // 판이 커질 수 있는 최대 크기

#define RANK_FILE "ranking.txt" // 랭킹을 저장할 파일 이름
#define NAME_LEN 12 // 이름 배열의 크기 (널 문자 포함)
#define RANK_MAX 5 // 화면에 보여줄 순위의 개수

//  ANSI 이스케이프 문자열. 화면에 출력하면 터미널이 글자가 아니라 명령으로 해석한다.
#define CLEAR_SCREEN "\033[2J\033[H" // 화면을 모두 지우고 커서를 좌상단으로 옮긴다
#define HIDE_CURSOR  "\033[?25l" // 터미널의 깜빡이는 커서를 숨긴다
#define SHOW_CURSOR  "\033[?25h" // 숨긴 커서를 다시 보이게 한다

//  ANSI 색상 코드. 위의 CLEAR_SCREEN 과 같은 종류이며, 글자의 색과 굵기를 바꾼다.
#define C_RESET   "\033[0m"    // 색을 원래대로 되돌린다. 색을 켰으면 반드시 짝을 맞춰 꺼야 한다
#define C_TITLE   "\033[1;36m" // 제목 : 밝은 하늘색
#define C_CURSOR  "\033[1;36m" // 커서가 있는 칸의 괄호
#define C_ANSWER  "\033[1;32m" // 정답 타일 * : 초록
#define C_UP      "\033[1;33m" // 내가 뒤집은 타일 0 : 노랑
#define C_OK      "\033[1;32m" // 성공 메시지
#define C_FAIL    "\033[1;31m" // 실패 메시지 : 빨강
#define C_INFO    "\033[2m"    // 조작 안내 : 흐리게. 눈이 판에 먼저 가도록 한다

typedef enum { TILE_DOWN, TILE_UP }TileState; // 타일 상태(UP: 뒤집어서 공개한 상태, DOWN: 미공개 상태)
typedef enum { TILE_NORMAL, TILE_ANSWER }TileType; // 타일 종류(NORMAL: 빈 타일, ANSWER: 정답 타일)
typedef enum { ANSWER_HIDE, ANSWER_SHOW }DrawMode; // 판을 그릴 때 정답을 보여줄지 여부

enum
{
    KEY_UP = 'w', KEY_DOWN = 's', KEY_LEFT = 'a', KEY_RIGHT = 'd',
    KEY_SPACE = ' ', KEY_ENTER = '\n', KEY_QUIT = 'q', KEY_RANK = 'r',
    KEY_BACKSPACE = 127 // 맥 터미널의 Delete 키가 보내는 값(아스키 DEL). 문자로 쓸 수 없어 숫자를 적는다
};//  키 코드. 모든 키가 문자 하나이므로 아스키 값을 그대로 쓴다.

typedef struct _Tile    //타일 정보
{
    TileState state; // 타일 상태 (TILE_DOWN, TILE_UP)
    TileType type; // 타일이 정답인지, 아닌지. (TILE_NORMAL, TILE_ANSWER)
}Tile;

typedef struct _Board    //보드 정보
{
    int size; // 한 변 길이
    Tile** grid; // 판. 실행 중에 크기를 정하므로 동적 할당한다
    int cursorX; // 열
    int cursorY; // 행
}Board;

typedef struct _RankNode    // 랭킹 한 줄
{
    char name[NAME_LEN]; // 플레이어 이름
    int score; // 최종 점수
    int round; // 도달 라운드
    struct _RankNode* next; // 다음 노드의 주소. 마지막 노드면 NULL
}RankNode;

struct termios original_terminal_setting; // 프로그램 시작 시의 터미널 설정 백업. 종료할 때 이 설정으로 되돌린다.

/* ---------- [1] 함수 원형 선언부 ---------- */

//  아래 목록은 함수가 처음 실행되는 순서대로 늘어놓았다. 위에서 아래로 읽으면
//  게임이 진행되는 순서가 그대로 보인다. 정의부도 같은 순서로 두었다.

void SET_terminal(void); 
// ㄴ터미널을 게임용으로 준비하는 함수
//  > 1. 현재 설정을 original_terminal_setting 에 백업한다.
//  > 2. 엔터 없이 키 하나씩 즉시 입력받도록 설정을 바꾸고 커서를 숨긴다.

int SHOW_menu(void); 
// ㄴ시작 화면을 보여주고 선택을 받는 함수
//  > 게임을 시작하면 1, 종료를 고르면 0 을 반환한다.

int READ_key(void); 
// ㄴ키 하나를 엔터 없이 즉시 입력받아 코드로 반환하는 함수
//  > 1바이트를 읽어 그 문자를 반환하고, 읽기에 실패하면 -1 을 반환한다.

RankNode* LOAD_ranking(void); 
// ㄴ랭킹 파일을 읽어 연결 리스트로 만드는 함수
//  > 파일이 없으면(첫 실행이면) NULL 을 반환한다.

void SHOW_ranking(const RankNode* head); 
// ㄴ랭킹을 화면에 보여주고 아무 키나 기다리는 함수

void FREE_ranking(RankNode* head); 
// ㄴ랭킹 리스트의 노드를 전부 반납하는 함수

void PLAY_game(int* outScore, int* outRound); 
// ㄴ게임 한 판을 처음부터 끝까지 진행하는 함수
//  > 끝났을 때의 점수와 도달 라운드를 인자로 받은 주소에 담아 준다.

int CALC_size(int round); 
// ㄴ라운드에 맞는 판 크기를 계산하는 함수.
//  > 두 라운드마다 한 칸씩 커지며, MAX_SIZE 를 넘지 않는다.

int CALC_answers(int round); 
// ㄴ라운드에 맞는 정답 타일 개수를 계산하는 함수

int CREATE_BoardMemory(Board* b, int size); 
// ㄴ동적할당: size x size 판의 메모리를 확보하는 함수
//  > 성공하면 1, 메모리가 부족하면 0 을 반환한다.

void FREE_BoardMemory(Board* b); 
// ㄴ판에 쓴 메모리를 전부 반납하는 함수

void RESET_board(Board* b); 
// ㄴ판을 시작 상태로 초기화. 즉 모든 타일을 숨김

void PLACE_answers(Board* b, int count); 
// ㄴ정답 타일을 무작위 위치에 count 개 배치하는 함수
//  > 이미 정답인 칸이 뽑히면 버리고 다시 뽑아 중복을 막는다.

void SHOW_answers(const Board* b, int seconds); 
// ㄴ정답을 seconds 초 동안 보여준 뒤 숨기는 함수
//  > 보여주는 동안 눌린 키는 전부 버린다.

void DRAW_board(const Board* b, DrawMode mode); 
// ㄴ판 전체를 화면에 출력하는 함수
//  > mode 가 ANSWER_SHOW 면 정답 타일을 * 로 함께 표시한다.

void SLEEP_ms(long ms); 
// ㄴms 밀리초 동안 프로그램을 멈추는 함수
//  > unistd.h 의 usleep 을 쓸 수 없으므로 time.h 의 nanosleep 으로 직접 만들었다.

void FLIP_tile(Board* b); 
// ㄴ커서가 있는 칸의 타일을 뒤집는 함수
//  > 가려진 칸은 공개하고, 공개된 칸은 다시 가린다.

void MOVE_cursor(Board* b, int key); 
// ㄴ눌린 키에 따라 커서를 한 칸 옮기는 함수
//  > 판 밖으로 나가는 이동은 무시한다.

int isMATCH_board(const Board* b); 
// ㄴ플레이어가 뒤집은 칸과 정답 칸이 완전히 일치하는지 검사하는 함수
//  > 일치하면 1, 하나라도 다르면 0 을 반환한다.

void SHOW_result(const Board* b, int correct); 
// ㄴ판정 결과와 정답 위치를 잠시 보여주는 함수

void SHOW_gameover(int score, int round); 
// ㄴ게임오버 화면을 보여주고 아무 키나 기다리는 함수

void SAVE_score(int score, int round); 
// ㄴ이번 판의 점수를 랭킹에 반영하고 보여주는 함수

void INPUT_name(char* name); 
// ㄴ이름을 한 글자씩 입력받아 인자로 받은 자리에 채우는 함수
//  > 영문과 숫자만 NAME_LEN - 1 글자까지 받고, 아무것도 입력하지 않으면 "NONAME" 을 넣는다.

RankNode* ADD_ranking(RankNode* head, const char* name, int score, int round);
//  > 점수 내림차순 자리에 끼워 넣고, 바뀔 수 있는 새 head 를 반환한다.
//  > 반환값을 반드시 head 에 다시 대입해야 한다.

void SAVE_ranking(const RankNode* head); 
// ㄴ랭킹 리스트를 파일에 저장하는 함수
//  > 상위 RANK_MAX 개만 쓰고, 기존 파일 내용은 지워진다.

void RESTORE_terminal(void); 
// ㄴ터미널을 백업해 둔 원래 설정으로 되돌리는 함수

/* ---------- [2] main 문 ---------- */
int main(void)
{
    int score = 0; // 점수 초기화
    int round = 1; // 라운드 초기화

    srand((unsigned int)time(NULL)); // 난수의 씨앗을 현재 시각으로 정한다.

    SET_terminal(); // 터미널 세팅: 여기서부터 키가 엔터 없이 즉시 들어온다.

    while (SHOW_menu() == 1)
    {
        PLAY_game(&score, &round);
        SHOW_gameover(score, round);
        SAVE_score(score, round); // 이름을 받아 랭킹에 반영하고 순위를 보여준다
    }

    RESTORE_terminal(); // 터미널을 원래대로 되돌린다. 빠뜨리면 종료 후 터미널이 먹통이 된다
    printf(CLEAR_SCREEN);

    return 0;
}

/* ---------- [3] 터미널 설정 함수 ---------- */
void SET_terminal(void)
{
    struct termios new_setting; // 설정을 조립할 작업용 사본

    setvbuf(stdin, NULL, _IONBF, 0);
    // ㄴgetchar 가 키를 미리 여러 개 모아 두지 않게 한다. 모아 두면 그 키들은
    //   tcflush 로 버릴 수 없어서, 기다리는 동안 눌린 키가 나중에 쏟아진다.

    tcgetattr(TILE_STDIN, &original_terminal_setting); // 현재 설정(TILE_STDIN)을 original_terminal_setting 으로 복사해 통째로 백업한다
    new_setting = original_terminal_setting; // 백업본을 복사해 필요한 항목만 끈다

    new_setting.c_lflag = new_setting.c_lflag & ~(ICANON | ECHO);
    // ㄴICANON(엔터까지 모아서 주기)과 ECHO(누른 키를 화면에 보여주기)를 끈다

    new_setting.c_cc[VMIN] = 1; // 최소 1바이트가 들어오면 곧바로 read 함수를 끝낸다
    new_setting.c_cc[VTIME] = 0; // 시간 제한은 두지 않는다

    tcsetattr(TILE_STDIN, TCSANOW, &new_setting); // 바꾼 설정(new_setting)을 지금 즉시(TCSANOW) 터미널(TILE_STDIN)에 적용한다

    printf(HIDE_CURSOR);
}

/* ---------- [4] 시작 화면 함수 ---------- */
int SHOW_menu(void)
{
    int key;
    RankNode* head;

    while (1)
    {
        printf(CLEAR_SCREEN);
        printf("\n\n");
        printf(C_TITLE "      T I L E   G A M E" C_RESET "\n\n\n");
        printf(C_OK    "      엔터 : 게임 시작" C_RESET "\n");
        printf(        "      r    : 랭킹 보기\n");
        printf(C_INFO  "      q    : 종료" C_RESET "\n");

        fflush(stdout);

        tcflush(TILE_STDIN, TCIFLUSH);

        key = READ_key();

        if (key == KEY_ENTER)
            return 1; // 게임 시작

        if (key == KEY_QUIT || key == -1)
            return 0; // 게임 종료

        if (key == KEY_RANK)    // 랭킹을 보여준 뒤 반복문을 돌아 메뉴를 다시 그린다
        {
            head = LOAD_ranking();
            SHOW_ranking(head);
            FREE_ranking(head);
        }
    }
}

/* ---------- [5] 키 입력 함수 ---------- */
int READ_key(void)
{
    int c; // 읽은 1바이트. EOF 와 구분해야 하므로 char 가 아니라 int 로 받는다

    c = getchar(); // raw 모드라서 엔터를 기다리지 않고 한 글자가 즉시 들어온다

    if (c == EOF)
        return -1;
        // ㄴ입력이 끊긴 것이므로 -1 을 반환한다.

    return c;
}

/* ---------- [6] 랭킹 파일 읽기 함수 ---------- */
RankNode* LOAD_ranking(void) // 파일에서 값을 읽고, 노드를 만들어 노드에 값을 저장한 다음, 노드들을 연결하는 함수
{
    FILE* fp;
    RankNode* head = NULL; // 머리 노드
    RankNode* tail = NULL; // 꼬리 노드
    RankNode* node;        // 새로 만들 노드
    
    char name[NAME_LEN];
    int score, round;
    
    fp = fopen(RANK_FILE, "r");
    
    if (fp == NULL) // 파일이 없는 경우
        return NULL;
    
    while (fscanf(fp, "%11s %d %d", name, &score, &round) == 3) // 반복 조건: fscanf는 파일에서 성공적으로 읽은 값의 개수를 반환.
    {
        node = (RankNode*)malloc(sizeof(RankNode)); // 동적할당으로 새로 만들 노드 메모리 할당
        
        if (node == NULL) // 동적할당 실패시 멈춤
            break;
        
        strncpy(node -> name, name, NAME_LEN - 1);
        node -> name[NAME_LEN - 1] = '\0';
        node -> score = score; // 파일에서 읽은 점수 대입
        node -> round = round; // 파일에서 읽은 라운드 대입
        node -> next = NULL;   // 쓰레기값 말고 NULL 대입
        
        if (head == NULL)
            head = node;        // 첫 노드
        else
            tail -> next = node;  // 두 번째 이후
        
        tail = node;
        
    }
    
    fclose(fp);
        
    return head;
}

/* ---------- [7] 랭킹 화면 표시 함수 ---------- */
void SHOW_ranking(const RankNode* head) // 헤드를 읽고, 헤드부터 연결된 노드들을 차례대로 읽어들여 값을 출력
{
    const RankNode* cur = head;   // 순회용 노드: 헤드 부터 시작
    int rank = 1;
    
    printf(CLEAR_SCREEN);
    printf("\n\n");
    printf(C_TITLE "      R A N K I N G" C_RESET "\n\n\n");

    if (head == NULL)
        printf("      기록이 없습니다.\n");
    
    while (cur != NULL && rank <= RANK_MAX)
    {
        if (rank == 1)
            printf(C_UP); // 1등만 노랑으로 강조한다

        printf("      %d.  %-12s  %5d 점    라운드 %d" C_RESET "\n", rank, cur -> name, cur -> score, cur -> round);
            // ㄴ1등이 아닐 때도 리셋을 찍지만, 색이 안 켜진 상태의 리셋은 아무 일도 하지 않는다.

        cur = cur -> next;
        rank = rank + 1;
    }
    
    printf("\n\n      메뉴로 돌아가고 싶으면 아무 키나 누르세요.\n");

    fflush(stdout);

    tcflush(TILE_STDIN, TCIFLUSH);
    READ_key();
}

/* ---------- [8] 랭킹 리스트 해제 함수 ---------- */
void FREE_ranking(RankNode* head)
{
    RankNode* cur = head;
    RankNode* next;      // 임시 보관용

    while (cur != NULL)
    {
        next = cur -> next;   // 1 지우기 전에 다음 주소를 챙긴다
        free(cur);            // 2 지운다
        cur = next;           // 3 챙겨 둔 주소로 이동한다
    }
}

/* ---------- [9] 게임 한 판 진행 함수 ---------- */
void PLAY_game(int* outScore, int* outRound)
{
    Board b;
    int key = 0; // 마지막에 누른 키의 코드
    int score = 0; // 지금까지 얻은 점수
    int lives = 3; // 남은 목숨
    int round = 1; // 현재 라운드
    
    int size = SIZE; // 첫 라운드 사이즈: 3
    int answers = SIZE; // 첫 라운드 정답 개수: 3

    while (lives > 0 && key != KEY_QUIT) // 라운드 반복. 목숨이 다하거나 q 를 누르면 끝난다
    {
        size = CALC_size(round); // 이번 라운드의 판 크기를 구한다. 라운드가 오를수록 커진다
        answers = CALC_answers(round); // 이번 라운드의 정답 개수를 구한다. 정답도 함께 늘어난다

        
        if (CREATE_BoardMemory(&b, size) == 0) // 메모리를 못 얻으면 게임을 계속할 수 없다
            break;
        
        RESET_board(&b); // 지난 라운드의 흔적을 지운다
        PLACE_answers(&b, answers); // 이번 라운드의 정답 개수만큼 배치한다

        SHOW_answers(&b, 3); // 정답을 3초 동안 보여준 뒤 숨긴다

        key = 0; // 지난 라운드에서 누른 엔터가 남아 있으면 안 되므로 초기화한다

        while (key != KEY_ENTER && key != KEY_QUIT) // 한 라운드의 입력을 받는다
        {
            DRAW_board(&b, ANSWER_HIDE); // 1 현재 상태를 그린다
            printf(C_INFO "\n  라운드 %d (%dx%d)   점수 %d   목숨 " C_RESET C_FAIL "%d" C_RESET,
                   round, size, size, score, lives);
                // ㄴ목숨만 빨강으로 띄운다. 줄어드는 것이 눈에 들어와야 하는 값이다.
            fflush(stdout);

            key = READ_key(); // 2 키를 누를 때까지 기다린다

            switch (key) // 3 받은 키에 따라 상태를 바꾼다
            {
                case KEY_SPACE:
                    FLIP_tile(&b); // 커서가 있는 칸을 뒤집는다
                    break;

                case KEY_ENTER:
                    break; // 제출. 아무것도 안 해도 이 반복문의 조건이 거짓이 된다

                default:
                    MOVE_cursor(&b, key); // 그 밖의 키는 커서 이동으로 넘긴다
                    break;
            }
        }

        if (key == KEY_ENTER) // 제출로 빠져나온 경우에만 판정한다
        {
            if (isMATCH_board(&b))
            {
                score = score + round * 10; // 라운드가 높을수록 점수를 많이 준다
                round++;
                SHOW_result(&b, 1);
            }
            else
            {
                lives--; // 라운드는 그대로 두고 목숨만 깎는다
                SHOW_result(&b, 0);
            }
        }
        
        FREE_BoardMemory(&b); // 이번 라운드에 쓴 메모리를 반납한다
    }

    *outScore = score; // 결과를 부른 쪽에 전달한다
    *outRound = round;
}

/* ---------- [10] 라운드별 판 크기 계산 함수 ---------- */
int CALC_size(int round)
{
    int size = SIZE + (round - 1) / 2; // 두 라운드마다 한 칸씩 커진다

    if (size > MAX_SIZE) // 아무리 커져도 MAX_SIZE 를 넘지 않는다
        size = MAX_SIZE;

    return size;
}

/* ---------- [11] 라운드별 정답 개수 계산 함수 ---------- */
int CALC_answers(int round)
{
    return SIZE + round / 2; // 판이 커지는 라운드의 한 박자 뒤에 늘어난다
}

/* ---------- [12] 보드 생성 함수 ---------- */
int CREATE_BoardMemory(Board* b, int size)
{
    int y;

    b -> size = size;

    b -> grid = (Tile**)malloc(sizeof(Tile*) * size); // 행 포인터를 size 개 담을 자리

    if (b -> grid == NULL) // 메모리가 부족하면 여기서 포기한다
        return 0;

    for (y = 0; y < size; y++)
        b -> grid[y] = NULL;
        // ㄴ먼저 전부 NULL 로 채워 둔다. 도중에 실패해도 안전하게 반납할 수 있다.

    for (y = 0; y < size; y++)
    {
        b -> grid[y] = (Tile*)malloc(sizeof(Tile) * size); // y 행에 들어갈 칸 size 개

        if (b -> grid[y] == NULL)
        {
            FREE_BoardMemory(b); // 지금까지 잡아 둔 것을 전부 반납하고 포기한다
            return 0;
        }
    }

    return 1;
}

/* ---------- [13] 보드 해제 함수 ---------- */
void FREE_BoardMemory(Board* b)
{
    int y;

    if (b -> grid == NULL) // 이미 반납했거나 잡은 적이 없으면 할 일이 없다
        return;

    for (y = 0; y < b -> size; y++)
        free(b -> grid[y]); // 각 행을 먼저 반납한다

    free(b -> grid); // 행 포인터 배열은 나중에 반납한다

    b -> grid = NULL; // 반납한 주소를 실수로 다시 쓰지 않도록 지운다
}

/* ---------- [14] 보드 리셋 함수 ---------- */
void RESET_board(Board* b)
{
    int y, x;

    for (y = 0; y < b -> size; y++)
    {
        for (x = 0; x < b -> size; x++)
        {
            b -> grid[y][x].state = TILE_DOWN;  // 모든 타일을 미공개 상태로 변경
            b -> grid[y][x].type = TILE_NORMAL; // 모든 타일을 일반 타입으로 변경
        }
    }
    
    b -> cursorX = 0; // 커서를 왼쪽 위로 되돌린다
    b -> cursorY = 0;
}

/* ---------- [15] 정답 타일 배치 함수 ---------- */
void PLACE_answers(Board* b, int count)
{
    int placed = 0; // 지금까지 배치한 정답 타일의 개수
    int y, x;
    
    if (count > b -> size * b -> size) // 판의 칸 수보다 많이 배치할 수는 없다
        count = b -> size * b -> size;
    
    while (placed < count)
    {
        y = rand() % b -> size;
        x = rand() % b -> size;

        if (b -> grid[y][x].type == TILE_NORMAL) // 아직 정답이 아닌 칸일 때만 정답으로 설정
        {
            b -> grid[y][x].type = TILE_ANSWER;
            placed++;
        }
    }
}

/* ---------- [16] 시간 제한 동안 정답을 보여주는 함수 ---------- */
void SHOW_answers(const Board* b, int seconds)
{
    int i;

    for (i = seconds; i > 0; i--)
    {
        DRAW_board(b, ANSWER_SHOW);
        printf(C_ANSWER "\n\n  타일들을 기억하세요!  %d" C_RESET, i);
            // ㄴ정답 * 와 같은 초록을 쓴다. 지금 보이는 초록을 외우라는 뜻이 색으로 전달된다.
        fflush(stdout);

        SLEEP_ms(1000); // 1초(1000밀리초) 동안 멈춘다
    }

    tcflush(TILE_STDIN, TCIFLUSH);
    // ㄴ기다리는 동안 눌린 키가 버퍼에 쌓여 있으므로 전부 버린다.
}

/* ---------- [17] 보드 그리기 함수 ---------- */
void DRAW_board(const Board* b, DrawMode mode)
{
    int y, x;
    char face;  // 칸 안에 찍을 숫자
    const char* color; // 그 글자에 입힐 색. face 와 짝을 이룬다

    printf(CLEAR_SCREEN); // 화면 지우고 처음부터 다시 그리기
    printf(C_TITLE "  TILE GAME " C_RESET "\n\n");
        

    for (y = 0; y < b -> size; y++)
    {
        for (x = 0; x < b -> size; x++)
        {
            if (mode == ANSWER_SHOW && b -> grid[y][x].type == TILE_ANSWER)
            {
                face = '*'; // 정답 공개 모드이면서, 타일 타입이 정답이면 정답으로 출력한다.
                color = C_ANSWER;
            }
            else if (b -> grid[y][x].state == TILE_UP)
            {
                face = '0'; // 타일이 뒤집히면 0을 출력한다.
                color = C_UP;
            }
            else
            {
                face = ' ';
                color = C_RESET; // 빈 칸은 색을 입힐 것이 없다
            }

            if (y == b -> cursorY && x == b -> cursorX)
                printf("%s<%s%c%s>%s ", C_CURSOR, color, face, C_CURSOR, C_RESET);
            else
                printf("[%s%c%s] ", color, face, C_RESET);
        }
        printf("\n");
    }
    printf(C_INFO "\n w/a/s/d: 이동      스페이스: 뒤집기      엔터: 제출      q: 종료" C_RESET);
    
    fflush(stdout);
        // ㄴ출력 내용이 화면에 즉시 나타나도록 버퍼를 밀어낸다.
}

/* ---------- [18] 시간 지연 함수 ---------- */
void SLEEP_ms(long ms)
{
    struct timespec t; // 초와 나노초를 나눠 담는 구조체. time.h 에 정의되어 있다

    t.tv_sec  = ms / 1000;                  // 1000으로 나눈 몫이 초
    t.tv_nsec = (ms % 1000) * 1000000L;     // 나머지를 나노초로. 1밀리초 = 1000000나노초

    nanosleep(&t, NULL);
}

/* ---------- [19] 타일 뒤집기 함수 ---------- */
void FLIP_tile(Board* b)
{
    Tile* t = &b -> grid[b -> cursorY][b -> cursorX]; // 커서가 가리키는 칸

    if (t -> state == TILE_DOWN)
        t -> state = TILE_UP;
    else
        t -> state = TILE_DOWN;
}

/* ---------- [20] 커서 이동 함수 ---------- */
void MOVE_cursor(Board* b, int key)
{
    switch (key)
    {
        case KEY_UP:
            if (b -> cursorY > 0)
                b -> cursorY--; // 위로 한칸 이동
            break;

        case KEY_DOWN:
            if (b -> cursorY < b -> size - 1)
                b -> cursorY++; // 아래로 한칸 이동
            break;

        case KEY_LEFT:
            if (b -> cursorX > 0)
                b -> cursorX--; // 왼쪽으로 한칸 이동
            break;

        case KEY_RIGHT:
            if (b -> cursorX < b -> size - 1)
                b -> cursorX++; // 오른쪽으로 한칸 이동
            break;

        default:
            break; // 그 밖의 키는 무시
    }
}

/* ---------- [21] 정답 판정 함수 ---------- */
int isMATCH_board(const Board* b)
{
    int y, x;

    for (y = 0; y < b -> size; y++)
    {
        for (x = 0; x < b -> size; x++)
        {
            if (b -> grid[y][x].state == TILE_UP && b -> grid[y][x].type == TILE_NORMAL)
                return 0; // 정답이 아닌 칸을 뒤집었다

            if (b -> grid[y][x].state == TILE_DOWN && b -> grid[y][x].type == TILE_ANSWER)
                return 0; // 정답인 칸을 뒤집지 않았다
        }
    }

    return 1; // 모든 칸이 일치
}

/* ---------- [22] 판정 결과 표시 함수 ---------- */
void SHOW_result(const Board* b, int correct)
{
    DRAW_board(b, ANSWER_SHOW); // 정답 위치를 함께 보여준다

    if (correct == 1)
        printf(C_OK "\n\n  정답입니다!" C_RESET);
    else
        printf(C_FAIL "\n\n  틀렸습니다!" C_RESET);

    fflush(stdout);

    SLEEP_ms(1500); // 1.5초 동안 결과를 보여준다

    tcflush(TILE_STDIN, TCIFLUSH); // 그동안 눌린 키는 버린다
}

/* ---------- [23] 게임오버 화면 함수 ---------- */
void SHOW_gameover(int score, int round)
{
    printf(CLEAR_SCREEN);
    printf("\n\n");
    printf(C_FAIL "      G A M E   O V E R" C_RESET "\n\n\n");
    printf("      도달 라운드 : %d\n", round);
    printf("      최종 점수   : %d\n\n\n", score);
    printf("      점수를 기록하려면 아무 키나 누르세요.\n");

    fflush(stdout);

    tcflush(TILE_STDIN, TCIFLUSH); // 게임 중 눌린 키가 남아 있으면 화면이 그냥 넘어간다

    READ_key();
}

/* ---------- [24] 이번 판의 점수를 랭킹에 저장하는 함수 ---------- */
void SAVE_score(int score, int round)
{
    char name[NAME_LEN]; // 이번 판 플레이어의 이름
    RankNode* head;

    INPUT_name(name); // 0 이름을 입력받고

    head = LOAD_ranking();                          // 1 지금까지의 기록을 읽고
    head = ADD_ranking(head, name, score, round);   // 2 이번 기록을 끼워 넣고
    SAVE_ranking(head);                             // 3 파일에 다시 쓰고
    SHOW_ranking(head);                             // 4 결과를 보여주고
    FREE_ranking(head);                             // 5 반납한다
}

/* ---------- [25] 이름 입력 함수 ---------- */
void INPUT_name(char* name)
{
    int len = 0; // 지금까지 입력한 글자 수
    int key;

    name[0] = '\0'; // 빈 문자열로 시작한다

    while (1)
    {
        printf(CLEAR_SCREEN);
        printf("\n\n");
        printf("      이름을 입력하고 엔터를 누르세요.\n\n");
        printf(C_INFO "      영문과 숫자 %d글자만 입력 가능하며. 공백은 사용할 수 없습니다.\n", NAME_LEN - 1);
        printf("      지우려면 백스페이스를 누르세요." C_RESET "\n\n\n");
            // ㄴ백스페이스는 동작하는데 안내가 없어 12단계에서 추가했다
        printf("      > " C_OK "%s" C_RESET "_\n", name); // ECHO 를 꺼 두었으므로 입력한 글자를 직접 그린다. _ 는 커서 대신 찍는 표시다

        fflush(stdout);

        key = READ_key();

        if (key == KEY_ENTER || key == -1) // 입력을 끝낸다. -1 은 입력이 끊긴 경우다
            break;

        if (key == KEY_BACKSPACE || key == 8) // 마지막 글자를 지운다
        {
            if (len > 0) // 빈 상태에서 더 지우면 배열 앞으로 벗어난다
            {
                len--;
                name[len] = '\0'; // 글자를 지우는 것이 아니라 끝 표시를 앞으로 당긴다
            }
        }
        else if (key > ' ' && key < 127 && len < NAME_LEN - 1)
        {
            // ㄴ공백(32)과 제어 문자를 막는다. 이름에 공백이 들어가면 파일 형식이 깨진다.
            //  ㄴ한글은 1바이트로 읽으면 음수가 되므로 여기서 함께 걸러진다.

            name[len] = (char)key; // 글자를 뒤에 붙인다
            len++;
            name[len] = '\0';      // 새 끝에 널을 다시 찍는다
        }
    }

    if (name[0] == '\0') // 아무것도 입력하지 않았다면 기본 이름을 쓴다
        strcpy(name, "NONAME");
        // ㄴ빈 이름을 그대로 저장하면 파일에 빈 칸이 생겨 다음 실행에서 랭킹이 깨진다.
}

/* ---------- [26] 랭킹에 새 기록을 끼워 넣는 함수 ---------- */
RankNode* ADD_ranking(RankNode* head, const char* name, int score, int round)
{
    RankNode* node; // 새로 만들 노드
    RankNode* cur;  // 들어갈 자리를 찾아다닐 노드

    node = (RankNode*)malloc(sizeof(RankNode));

    if (node == NULL) // 메모리를 못 얻으면 원래 리스트를 그대로 돌려준다
        return head;

    strncpy(node -> name, name, NAME_LEN - 1);
    node -> name[NAME_LEN - 1] = '\0';
    node -> score = score;
    node -> round = round;
    node -> next = NULL;

    if (head == NULL || score > head -> score) // 첫 기록이거나 1등인 경우
    {
        node -> next = head; // 원래 1등이 내 뒤로 온다. 비어 있었다면 NULL 이 들어간다
        return node;         // 새 노드가 새 head 다
    }

    cur = head;

    while (cur -> next != NULL && cur -> next -> score >= score)
        cur = cur -> next;
        // ㄴ내 점수보다 높거나 같은 노드를 지나친다. 멈춘 자리가 내 바로 앞 노드다.

    node -> next = cur -> next; // 내 뒤를 먼저 잇는다
    cur -> next = node;         // 그 다음에 앞을 잇는다

    return head; // head 는 안 바뀌었으므로 받은 것을 그대로 돌려준다
}

/* ---------- [27] 랭킹 파일 저장 함수 ---------- */
void SAVE_ranking(const RankNode* head)
{
    FILE* fp;
    const RankNode* cur = head;
    int count = 0; // 지금까지 쓴 줄 수

    fp = fopen(RANK_FILE, "w"); // 쓰기 모드. 파일이 없으면 만들고, 있으면 내용을 전부 지운다

    if (fp == NULL) // 쓸 수 없어도 게임은 계속되어야 하므로 조용히 돌아간다
        return;

    while (cur != NULL && count < RANK_MAX) // 상위 RANK_MAX 개만 남긴다
    {
        fprintf(fp, "%s %d %d\n", cur -> name, cur -> score, cur -> round);

        cur = cur -> next;
        count++;
    }

    fclose(fp); // 닫아야 실제로 파일에 써진다
}

/* ---------- [28] 터미널 복구 함수 ---------- */
void RESTORE_terminal(void)
{
    printf(C_RESET); // 색을 켠 채로 끝나면 종료 후에도 터미널 글자색이 그대로 남는다
    printf(SHOW_CURSOR);

    tcsetattr(TILE_STDIN, TCSANOW, &original_terminal_setting);
    // ㄴ백업해 둔 원래 설정을 그대로 다시 적용한다.
}


/* 메모

struct termios          // termios.h 에 이미 정의되어 있다
{
    ? c_iflag;    // 입력 가공 (개행 문자 변환 등)
    c_oflag;    // 출력 가공
    c_cflag;    // 통신 속도 등 하드웨어 설정
    c_lflag;    // 줄 편집, 에코 (local flags)           ICANON, ECHO 를 끈다
    c_cc[];     // 특수 문자와 제어 값 배열                VMIN, VTIME 을 정한다
};

1. TILE_STDIN(터미널의 현재 설정) -> original_terminal_setting (백업 저장) 터미널 설정 건드리는

2. original_terminal_setting -> new_setting (작업용 사본) OK

3. new_setting 의 설정 바꾸기 (ICANON, ECHO 끄기 / VMIN, VTIME 정하기)

4. new_setting -> TILE_STDIN (바꾼 설정을 터미널에 적용)

5. 종료할 때 original_terminal_setting -> TILE_STDIN (원래대로 복구)

*/
