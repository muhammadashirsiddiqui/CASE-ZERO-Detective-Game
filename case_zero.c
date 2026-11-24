#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
/* Older Windows GCC headers may not define this Windows console flag. */
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#endif
#define N 8
#define RED "\033[31m"
#define GOLD "\033[33m"
#define CYAN "\033[36m"
#define RESET "\033[0m"
enum { CIVILIAN, MAFIA, DOCTOR, DETECTIVE };
typedef struct { char name[20]; int role, alive, suspicion, votes; } Player;
Player p[N] = {{"Arsalan",0,0,0,0},{"Moiz",0,0,0,0},{"Shanzay",0,0,0,0},{"Aniqa",0,0,0,0},{"Ayan",0,0,0,0},{"Aliza",0,0,0,0},{"Fasih",0,0,0,0},{"Fahad",0,0,0,0}};
int me, roundNo, score, known[N], difficulty;
const char *roles[]={"Civilian","Mafia","Doctor","Detective"};
const char *places[]={"University common room","Parking area","Canteen","Library corridor","Campus courtyard"};
time_t gameStarted;
int clockEnabled=0;
/* Pakistan Standard Time is UTC+05:00 year-round. Explicit UTC offset
   ensures the same wall clock on Windows and Linux regardless of OS zone. */
struct tm *pakistanTime(time_t now){
    return gmtime(&(time_t){ now + 5 * 60 * 60 });
}
/* The clock stays on the first terminal row and ticks while waiting for input.
   select() avoids extra threads and works in a normal Linux terminal. */
void timerBar(void){
    time_t now=time(NULL); long seconds=(long)difftime(now,gameStarted);
    struct tm *t=pakistanTime(now); char stamp[20]="--:--:-- --";
    if(t)strftime(stamp,sizeof(stamp),"%I:%M:%S %p",t);
    printf("\033[s\033[1;1H\033[2K" CYAN "[ CASE ZERO | PKT %s | SESSION %02ld:%02ld:%02ld ]" RESET "\033[u",stamp,seconds/3600,(seconds/60)%60,seconds%60);
    fflush(stdout);
}
/* GUI bridge: line-based input and explicit prompts, no console required. */
#ifdef CASE_GUI
void enableTerminal(void){}
int nextChar(void){return getchar();}
#else
/* Portable input: refresh the clock while waiting for keyboard input. */
#ifdef _WIN32
void enableTerminal(void){
    HANDLE output=GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode=0;
    if(output!=INVALID_HANDLE_VALUE && GetConsoleMode(output,&mode))
        SetConsoleMode(output,mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
int nextChar(void){
    static int count=0;
    time_t lastClock=time(NULL);
    for(;;){
        if(_kbhit()){
            int ch=_getch();
            /* Ignore arrow keys and function keys. */
            if(ch==0||ch==224){_getch();continue;}
            if(ch==3)exit(0);
            if(ch=='\r'||ch=='\n'){
                putchar('\n');fflush(stdout);count=0;return '\n';
            }
            if(ch=='\b'){
                if(count>0){fputs("\b \b",stdout);fflush(stdout);count--;return '\b';}
                continue;
            }
            if(ch>=32&&ch<=126){putchar(ch);fflush(stdout);count++;return ch;}
            continue;
        }
        Sleep(100);
        if(clockEnabled && time(NULL)!=lastClock){lastClock=time(NULL);timerBar();}
    }
}
#else
void enableTerminal(void){}
int nextChar(void){
    for(;;){
        fd_set rfds;struct timeval delay={1,0};int rc;char ch;
        FD_ZERO(&rfds);FD_SET(STDIN_FILENO,&rfds);
        rc=select(STDIN_FILENO+1,&rfds,NULL,NULL,&delay);
        if(rc==0){if(clockEnabled)timerBar();continue;}
        if(rc<0){if(errno==EINTR)continue;return -1;}
        rc=(int)read(STDIN_FILENO,&ch,1);
        return rc==1?(unsigned char)ch:-1;
    }
}
#endif
#endif /* CASE_GUI */
void readLine(char *s,int size){int n=0,ch;while((ch=nextChar())>=0){if(ch=='\n')break;if(ch=='\r')continue;if(ch=='\b'){if(n>0)n--;continue;}if(n<size-1)s[n++]=(char)ch;}s[n]='\0';if(ch<0&&n==0)exit(0);}
void screen(void){
#ifdef CASE_GUI
    puts("\n@@CZ_SCREEN@@");
#endif
    printf("\033[2J\033[H\n" RED "======================================================\n             C A S E  Z E R O\n" RESET "              THE MYSTERY FILES | KARACHI\n" RED "======================================================\n" RESET);if(clockEnabled)timerBar(); }
int input(int lo,int hi){ char line[100], extra; int v; for(;;){
#ifdef CASE_GUI
    printf("\n@@CZ_INPUT:%d:%d@@\n",lo,hi);fflush(stdout);
#else
    printf(GOLD "> " RESET);fflush(stdout);
#endif
    readLine(line,sizeof line); if(sscanf(line,"%d %c",&v,&extra)==1 && v>=lo && v<=hi)return v; puts("Invalid choice. Try again."); } }
void pauseGame(void){char line[100];
#ifdef CASE_GUI
puts("\n@@CZ_PAUSE@@");fflush(stdout);
#else
printf("\nPress Enter to continue...");fflush(stdout);
#endif
readLine(line,sizeof line);}
void roster(void){int i;puts("\n ID  NAME          STATUS      SUSPICION"); for(i=0;i<N;i++){printf(" %d   %-12s  %-10s  ",i+1,p[i].name,p[i].alive?"Alive":"Out");if(!p[i].alive)printf("%s",roles[p[i].role]);else if(i==me)printf("YOU / %s",roles[p[i].role]);else {int k;for(k=0;k<p[i].suspicion;k++)printf("#");if(known[i])printf(" [verified: %s]",p[i].role==MAFIA?"MAFIA":"NOT MAFIA");}puts("");}}
int eligible(int i,int exclude,int nonmafia){return p[i].alive && i!=exclude && (!nonmafia || p[i].role!=MAFIA);}
int pick(int exclude,int nonmafia){int ids[N],n=0,i;for(i=0;i<N;i++)if(eligible(i,exclude,nonmafia))ids[n++]=i;return n?ids[rand()%n]:-1;}
int choose(int exclude,int nonmafia){int i,v;for(i=0;i<N;i++)if(eligible(i,exclude,nonmafia))printf(" %d. %s\n",i+1,p[i].name);for(;;){v=input(1,N)-1;if(eligible(v,exclude,nonmafia))return v;puts("Choose an eligible player.");}}
void suspicion(int i,int delta){p[i].suspicion+=delta;if(p[i].suspicion<0)p[i].suspicion=0;if(p[i].suspicion>9)p[i].suspicion=9;}
int winner(void){int i,m=0,c=0;for(i=0;i<N;i++)if(p[i].alive){if(p[i].role==MAFIA)m++;else c++;}return m==0?1:m>=c?2:0;}
void init(int role){int i,j,bag[N]={MAFIA,MAFIA,DOCTOR,DETECTIVE,CIVILIAN,CIVILIAN,CIVILIAN,CIVILIAN};for(i=N-1;i>0;i--){j=rand()%(i+1);int t=bag[i];bag[i]=bag[j];bag[j]=t;}if(role){for(i=0;i<N;i++)if(bag[i]==role-1)break;int t=bag[me];bag[me]=bag[i];bag[i]=t;}for(i=0;i<N;i++){p[i].role=bag[i];p[i].alive=1;p[i].suspicion=0;known[i]=0;}score=0;roundNo=0;}
int night(void){int i,attack,protect=-1,detect=-1;screen();printf(CYAN "NIGHT %02d | The lights go out.\n" RESET,roundNo);puts("A voice note arrives: 'Kisi par bharosa mat karna.'");attack=pick(-1,1);if(p[me].alive&&p[me].role==MAFIA){puts("Choose tonight's target:");attack=choose(me,1);}for(i=0;i<N;i++)if(p[i].alive&&p[i].role==DOCTOR){if(i==me){puts("Choose someone to protect (yourself allowed):");protect=choose(-1,0);}else protect=pick(-1,0);}for(i=0;i<N;i++)if(p[i].alive&&p[i].role==DETECTIVE){if(i==me){puts("Secret background check - choose a player:");detect=choose(me,0);known[detect]=1;printf("Private report: %s is %s.\n",p[detect].name,p[detect].role==MAFIA?"MAFIA":"NOT MAFIA");score+=5;}else {detect=pick(i,0);if(detect>=0&&p[detect].role==MAFIA)suspicion(detect,2);}}puts("\nThe generator starts. Someone runs down the corridor...");if(attack==protect){puts("A doctor intervened. Nobody was eliminated tonight.");if(protect==me)score+=10;attack=-1;}else if(attack>=0){p[attack].alive=0;printf(RED "At dawn, %s is missing. Role: %s.\n" RESET,p[attack].name,roles[p[attack].role]);}pauseGame();return attack;}
void day(int victim){int i,j,k,real=-1,decoy=-1,used[3]={0},moves= difficulty==1?3:difficulty==2?2:1,choice;for(i=0;i<N;i++)if(p[i].alive&&p[i].role==MAFIA){if(real<0||rand()%2==0)real=i;}decoy=pick(-1,1);screen();printf(GOLD "DAY %02d | %s\n" RESET,roundNo,places[(roundNo-1)%5]);if(victim>=0)printf("An abandoned phone belonging to %s is found.\n",p[victim].name);else puts("A torn note is found under the door.");puts("The campus group chat is exploding. Nobody agrees on what happened.");for(k=0;k<3;k++){i=pick(-1,0);if(i<0)break;j=pick(i,0);if(j<0)break;if(p[i].role==MAFIA && p[j].role==MAFIA)j=pick(i,1);if(j<0)continue;printf("%s: '%s par mujhe shak hai. Kal raat kahan thay?'\n",p[i].name,p[j].name);suspicion(j,1);}roster();if(p[me].alive){while(moves>0){printf("\nInvestigation moves: %d\n1. Recover CCTV fragment\n2. Read anonymous WhatsApp tip\n3. Decode access log\n4. Accuse someone publicly\n5. Defend someone\n6. Proceed to vote\n",moves);choice=input(1,6);if(choice==6)break;if(choice<=3&&used[choice-1]){puts("Already examined this round.");continue;}if(choice<=3)used[choice-1]=1;switch(choice){case 1:if(real>=0&&decoy>=0)printf("CCTV: %s and %s were near the corridor. The camera loses signal.\n",p[roundNo%2?real:decoy].name,p[roundNo%2?decoy:real].name);if(real>=0)suspicion(real,1);if(decoy>=0)suspicion(decoy,1);score+=3;break;case 2:if(decoy>=0&&real>=0){i=rand()%3?decoy:real;printf("Unknown sender: '%s did it!' Sender identity cannot be verified.\n",p[i].name);suspicion(i,2);}break;case 3:if(real>=0&&decoy>=0){int hour=21, minute=35+rand()%10, offset=3+rand()%5; printf("Recorder clock is %d minutes FAST. Raw event: %02d:%02d.\n",offset,hour,minute+offset); printf("Gate camera: %s at %02d:%02d; %s at %02d:%02d.\n",p[real].name,hour,minute,p[decoy].name,hour,minute-2); printf("Enter corrected minute (0-59):\n"); if(input(0,59)==minute){puts("Correct! First entry aligns with the restricted gate event. Not proof of guilt alone.");suspicion(real,2);score+=8;}else puts("Clock mismatch. Evidence remains unverified.");}break;case 4:puts("Who do you accuse?");i=choose(me,0);suspicion(i,3);printf("%s: 'Evidence dikhao, sirf ilzaam mat lagao!'\n",p[i].name);break;case 5:puts("Who do you defend?");i=choose(me,0);suspicion(i,-2);printf("%s: 'Shukriya. Lekin yahan sab ka apna agenda hai.'\n",p[i].name);break;}moves--;}}else puts("You are eliminated. Spectator mode: the remaining players decide.");pauseGame();}
void vote(void){int i,j,target,best,top=-1,tie=0;screen();puts(GOLD "THE VOTE | A decision must be made.\n" RESET);roster();for(i=0;i<N;i++)p[i].votes=0;for(i=0;i<N;i++)if(p[i].alive){if(i==me){puts("Cast your vote:");target=choose(me,0);}else{target=-1;best=-999;for(j=0;j<N;j++)if(eligible(j,i,p[i].role==MAFIA)){int weight=p[j].suspicion*2+rand()%6;if(weight>best){best=weight;target=j;}}}if(target>=0){p[target].votes++;printf("%s votes for %s\n",p[i].name,p[target].name);}}target=-1;for(i=0;i<N;i++)if(p[i].alive){if(p[i].votes>top){top=p[i].votes;target=i;tie=0;}else if(p[i].votes==top)tie=1;}if(tie||target<0)puts("TIE! Nobody is eliminated. Another night awaits.");else{p[target].alive=0;printf(RED "\n%s has been voted out. The envelope opens...\n" RESET,p[target].name);printf("SECRET ROLE: %s\n",roles[p[target].role]);if(p[target].role==MAFIA&&p[me].role!=MAFIA)score+=20;if(p[target].role!=MAFIA&&p[me].role==MAFIA)score+=15;}pauseGame();}
void finish(int w){int i,win=(w==1&&p[me].role!=MAFIA)||(w==2&&p[me].role==MAFIA);FILE *f; if(win)score+=50;screen();printf("%s\n",w==1?"CAMPUS SAVED - TOWN WINS":w==2?"THE MAFIA TAKES CONTROL":"CASE UNRESOLVED - ROUND LIMIT");printf("Your team: %s | Score: %d | Rank: %s\n",win?"VICTORY":"DEFEAT / DRAW",score,score>=70?"Master Investigator":score>=40?"Inspector":"Rookie");for(i=0;i<N;i++)printf("%-12s %s\n",p[i].name,roles[p[i].role]);f=fopen("case_history.txt","a");if(f){fprintf(f,"%s | %s | score %d | rounds %d\n",p[me].name,win?"Win":"No win",score,roundNo);fclose(f);}pauseGame();}
int mafia(void){int choice,role,w,victim,i; srand((unsigned)time(NULL));for(;;){screen();puts("1. New game\n2. Rules\n3. Case history\n4. Return to game suite");choice=input(1,4);if(choice==4)return 0;if(choice==2){puts("Two Mafia hide among eight players. Doctor protects at night; Detective checks identities. Investigate clues and influence the public vote. Tips can be false. Town wins when all Mafia are out; Mafia wins at parity. If eliminated, spectate. Maximum 12 rounds.");pauseGame();continue;}if(choice==3){char line[200];FILE *f=fopen("case_history.txt","r");if(f){while(fgets(line,sizeof line,f))printf("%s",line);fclose(f);}else puts("No completed cases yet.");pauseGame();continue;}puts("Choose your character:");for(i=0;i<N;i++)printf("%d. %s\n",i+1,p[i].name);me=input(1,N)-1;puts("Choose role: 1 Civilian | 2 Mafia | 3 Doctor | 4 Detective | 5 Surprise");role=input(1,5);puts("Difficulty: 1 Easy | 2 Medium | 3 Hard");difficulty=input(1,3);init(role==5?0:role);screen();printf("You are %s. SECRET ROLE: %s\n",p[me].name,roles[p[me].role]);if(p[me].role==MAFIA)for(i=0;i<N;i++)if(i!=me&&p[i].role==MAFIA)printf("Your secret partner: %s\n",p[i].name);puts("Karachi, 9:47 PM. A fictional campus event ends in a blackout. Eight friends are locked inside. Two are hiding a secret.");pauseGame();w=0;while(!w&&roundNo<12){roundNo++;victim=night();w=winner();if(w)break;day(victim);vote();w=winner();}finish(w);}}

/* Independent mystery: all three recorded attributes must agree. */
void recordMode(const char *mode,int points){FILE *f=fopen("case_history.txt","a");if(f){fprintf(f,"%s | score %d\n",mode,points);fclose(f);}}
void murder(void){
    const char *rooms[]={"Library","Lab","Canteen","Parking"};
    int culprit=rand()%N, victim=(culprit+1+rand()%7)%N;
    int a=(culprit+2)%N,b=(culprit+4)%N,c=(culprit+6)%N;
    int room=rand()%4, minute=10+rand()%35, offset=3+rand()%7;
    int moves=5, seen[5]={0}, opt,i,answer,proof,points=0;
    /* Each suspect has a room, a card suffix and a verified time window.
       The culprit alone matches all three. The victim is not a suspect. */
    int r[N],card[N],t[N];
    for(i=0;i<N;i++){r[i]=(room+1+i%3)%4;card[i]=100+i;t[i]=minute+3+i;}
    r[culprit]=r[a]=r[b]=room;card[culprit]=card[a]=card[c]=200+rand()%600;
    t[culprit]=t[b]=t[c]=minute;
    screen();puts(GOLD "FILE 02 | THE MIDNIGHT MURDER" RESET);
    printf("11:58 PM. %s is found dead in a fictional campus. A recorder was tampered with.\n",p[victim].name);
    puts("A message: 'Jo nazar aa raha hai, zaroori nahi woh sach ho.'\nYour warrant expires after five investigations. No guessing without proof.");
    pauseGame();
    for(;;){printf(CYAN "\nCASE BOARD | Actions left: %d\n" RESET,moves);
        puts("1. Scene report\n2. Recover damaged CCTV\n3. Read card entry log\n4. Interview suspects\n5. Anonymous voice note\n6. Review collected evidence\n7. Final accusation\n8. Return to suite");
        opt=input(1,8);if(opt==8)return;if(opt==7)break;
        if(opt<=5){if(seen[opt-1]){puts("Already collected; review the board (6).");continue;}if(!moves){puts("Warrant expired. Review or accuse.");continue;}seen[opt-1]=1;moves--;}
        if(opt==1|| (opt==6&&seen[0]))printf("SCENE: %s. Incident time unknown; recorder damaged. Reader cached a card suffix: %d.\n",rooms[room],card[culprit]);
        if(opt==2|| (opt==6&&seen[1]))printf("CCTV: recorder is %d minutes FAST. Incident frame shows 23:%02d.\n",offset,minute+offset);
        if(opt==3|| (opt==6&&seen[2])){puts("CARD LOG (shared cards exist; a match alone is insufficient):");for(i=0;i<N;i++)if(i!=victim)printf("%-9s card %d\n",p[i].name,card[i]);}
        if(opt==4|| (opt==6&&seen[3])){puts("INTERVIEWS: verified independent location/time records:");for(i=0;i<N;i++)if(i!=victim)printf("%-9s | %-9s | 23:%02d | %s\n",p[i].name,rooms[r[i]],t[i],i%2?"Main gate witness":"Signed attendance record");}
        if(opt==5|| (opt==6&&seen[4]))printf("VOICE NOTE: 'Mujhe %s par shak hai!' Sender unknown; NOT authenticated.\n",p[a==victim?b:a].name);
    }
    puts("Select suspect by ID:");for(i=0;i<N;i++)if(i!=victim)printf("%d. %s\n",i+1,p[i].name);
    do{answer=input(1,N)-1;if(answer==victim)puts("Victim cannot be accused.");}while(answer==victim);
    puts("Which proof supports your accusation?\n1. Anonymous voice note\n2. Matching card only\n3. Location + corrected time + card\n4. Suspicious personality");proof=input(1,4);
    puts("Enter corrected CCTV minute (0-59):");i=input(0,59);
    if(answer==culprit&&proof==3&&i==minute&&seen[0]&&seen[1]&&seen[2]&&seen[3]){points=100;puts(CYAN "CASE SOLVED: evidence chain accepted." RESET);}else puts(RED "CASE FAILED: suspect or evidence chain insufficient." RESET);
    printf("Actual culprit: %s | %s | 23:%02d | card %d\nScore: %d\n",p[culprit].name,rooms[room],minute,card[culprit],points);recordMode("Midnight Murder",points);pauseGame();
}

/* Escape room: three linked puzzles plus a shared action budget. */
void escapeRoom(void){
    int digits[3],power[3]={0},done[3]={0},tries=12,choice,i,code,guess,points=0,hint=0;
    int start=2+rand()%5,step=2+rand()%3,base=10+rand()%15,delay=2+rand()%6;
    const char *words[]={"CODES","DOORS","LIGHT"}; int word=rand()%3;
    digits[0]=start+step*3;digits[0]%=10;digits[1]=(base-delay)%10;digits[2]=rand()%9+1;
    code=100*digits[0]+10*digits[1]+digits[2];
    screen();puts(GOLD "FILE 03 | ESCAPE THE DARKNESS" RESET);
    puts("The campus basement door locks. Emergency power is failing.\nThree stations hide a three-digit exit code. Every action costs battery.");
    while(tries>0){printf("\nBATTERY: %d/12 | Fragments: %d/3\n",tries,done[0]+done[1]+done[2]);
        puts("1. Electrical cabinet\n2. Security terminal\n3. Radio desk\n4. Review fragments\n5. Emergency hint\n6. Enter exit code\n7. Return to suite");choice=input(1,7);if(choice==7)return;
        if(choice==4){for(i=0;i<3;i++)if(done[i])printf("Digit %d = %d\n",i+1,digits[i]);continue;}
        if(choice<=3&&done[choice-1]){printf("Already solved: digit %d = %d\n",choice,digits[choice-1]);continue;}
        tries--;
        if(choice==1){puts("Restore the three switches: ON = 1, OFF = 0.\nEngineer note: first and third ON; middle OFF.");for(i=0;i<3;i++){printf("Switch %d: ",i+1);power[i]=input(0,1);}if(power[0]&& !power[1]&&power[2]){printf("Sequence: %d, %d, %d, ?\nEnter next number (0-99):\n",start,start+step,start+2*step);guess=input(0,99);if(guess==start+3*step)done[0]=1;else puts("Sequence incorrect.");}else puts("Circuit rejected.");}
        if(choice==2){printf("Log timestamp: 22:%02d. Clock runs %d minutes FAST.\nEnter actual minute: ",base,delay);if(input(0,59)==base-delay)done[1]=1;else puts("Time check failed.");}
        if(choice==3){printf("Radio message (each letter shifted forward one): ");for(i=0;i<5;i++)putchar(words[word][i]=='Z'?'A':words[word][i]+1);puts("");printf("Signal key: %d. Decode the word by moving each letter BACK one.\n1. CODES\n2. DOORS\n3. LIGHT\n",digits[2]);if(input(1,3)==word+1)done[2]=1;else puts("Radio rejects the word.");}
        if(choice<=3&&done[choice-1])printf(CYAN "Fragment unlocked: digit %d = %d\n" RESET,choice,digits[choice-1]);
        if(choice==5){hint++;puts("Cabinet: constant difference, keep last digit. Terminal: subtract clock delay, keep last digit. Radio: move each letter back one.");}
        if(choice==6){puts("Enter three-digit code:");guess=input(0,999);if(guess==code&&done[0]&&done[1]&&done[2]){points=70+tries*3-hint*5;puts(CYAN "DOOR OPEN. You escaped the blackout!" RESET);break;}puts("Access denied. All three fragments must be validated.");}
    }
    if(!points)printf(RED "Battery depleted. Exit code was %03d.\n" RESET,code);
    printf("Score: %d\n",points);recordMode("Escape the Darkness",points);pauseGame();
}
/* Startup digital clock: Pakistan time (UTC+5), 12-hour AM/PM. */
void startupClock(void){
    const char *digits[10][5]={
        {"###","# #","# #","# #","###"},
        {" # ","## "," # "," # ","###"},
        {"###","  #","###","#  ","###"},
        {"###","  #","###","  #","###"},
        {"# #","# #","###","  #","  #"},
        {"###","#  ","###","  #","###"},
        {"###","#  ","###","# #","###"},
        {"###","  #","  #","  #","  #"},
        {"###","# #","###","# #","###"},
        {"###","# #","###","  #","###"}
    };
    int remaining,row,i;
    char clockText[9],dateText[80],amPm[3];
#ifndef _WIN32
    struct timespec delay={1,0};
#endif
    for(remaining=5;remaining>0;remaining--){
        time_t now=time(NULL);
        struct tm *local=pakistanTime(now);
        if(local==NULL)return;
        strftime(clockText,sizeof clockText,"%I:%M:%S",local);
        strftime(amPm,sizeof amPm,"%p",local);
        strftime(dateText,sizeof dateText,"%A, %d %B %Y",local);
        printf("\033[2J\033[H" RED "======================================================\n"
               "                  C A S E  Z E R O\n"
               "                  THE MYSTERY FILES\n"
               "======================================================\n\n" RESET);
        printf("             PAKISTAN STANDARD TIME (12-HOUR | UTC+05:00)\n\n" CYAN);
        for(row=0;row<5;row++){
            printf("             ");
            for(i=0;clockText[i]!='\0';i++){
                if(clockText[i]==':')printf("%s ",(row==1||row==3)?" # ":"   ");
                else printf("%s ",digits[clockText[i]-'0'][row]);
            }
            putchar('\n');
        }
        printf(RESET "\n             %s  %s PKT\n             %s\n\n",clockText,amPm,dateText);
        printf(GOLD "Initializing investigation system... [%d]\n" RESET,remaining);
        printf("[");for(i=0;i<5;i++)putchar(i<6-remaining?'#':' ');puts("]");
        fflush(stdout);
#ifdef _WIN32
        Sleep(1000);
#else
        nanosleep(&delay,NULL);
#endif
    }
}


/* FILE 04 - SUDOKU LOCK: 9x9 puzzle plus genuine backtracking solver.
   Digits can occur once in each row, column and 3x3 box. */
int gridValid(int g[9][9],int row,int col,int value){
    int i,j,rs=row/3*3,cs=col/3*3;
    for(i=0;i<9;i++)if(g[row][i]==value||g[i][col]==value)return 0;
    for(i=rs;i<rs+3;i++)for(j=cs;j<cs+3;j++)if(g[i][j]==value)return 0;
    return 1;
}
int sudokuSolve(int g[9][9]){
    int r,c,k;
    for(r=0;r<9;r++)for(c=0;c<9;c++)if(g[r][c]==0){
        for(k=1;k<=9;k++)if(gridValid(g,r,c,k)){
            g[r][c]=k;if(sudokuSolve(g))return 1;g[r][c]=0;
        }
        return 0;
    }
    return 1;
}
/* Stop after finding two solutions: used to enforce unique playable boards. */
void countSolutions(int g[9][9],int *count){
    int r,c,k;
    if(*count>=2)return;
    for(r=0;r<9;r++)for(c=0;c<9;c++)if(!g[r][c]){
        for(k=1;k<=9;k++)if(gridValid(g,r,c,k)){
            g[r][c]=k;countSolutions(g,count);g[r][c]=0;
            if(*count>=2)return;
        }
        return;
    }
    ++*count;
}
void copyGrid(int dst[9][9],int src[9][9]){int i,j;for(i=0;i<9;i++)for(j=0;j<9;j++)dst[i][j]=src[i][j];}
void sudokuGenerate(int g[9][9],int solution[9][9],int holes){
    int r,c,i,j,k,n,perm[9],rows[9],cols[9],order[81],test[9][9],removed=0;
    /* Shift and shuffle digits and rows/columns inside their groups. */
    for(i=0;i<9;i++){perm[i]=i+1;rows[i]=i;cols[i]=i;order[i]=i;}
    for(i=80;i>=9;i--)order[i]=i;
    for(i=8;i>0;i--){j=rand()%(i+1);k=perm[i];perm[i]=perm[j];perm[j]=k;}
    for(i=0;i<9;i+=3){
        for(j=2;j>0;j--){k=rand()%(j+1);n=rows[i+j];rows[i+j]=rows[i+k];rows[i+k]=n;
            n=cols[i+j];cols[i+j]=cols[i+k];cols[i+k]=n;}
    }
    for(r=0;r<9;r++)for(c=0;c<9;c++)solution[r][c]=perm[(rows[r]*3+rows[r]/3+cols[c])%9];
    copyGrid(g,solution);
    for(i=80;i>0;i--){j=rand()%(i+1);k=order[i];order[i]=order[j];order[j]=k;}
    for(i=0;i<81&&removed<holes;i++){
        r=order[i]/9;c=order[i]%9;k=g[r][c];g[r][c]=0;
        copyGrid(test,g);n=0;countSolutions(test,&n);
        if(n!=1)g[r][c]=k;else removed++;
    }
}
void sudokuPrint(int g[9][9],int fixed[9][9]){
    int r,c;
    puts("        1 2 3   4 5 6   7 8 9");
    puts("      +-------+-------+-------+");
    for(r=0;r<9;r++){
        printf("   %d  | ",r+1);
        for(c=0;c<9;c++){
            if(!g[r][c])printf(". ");
            else if(fixed[r][c])printf(CYAN "%d " RESET,g[r][c]);
            else printf(GOLD "%d " RESET,g[r][c]);
            if(c%3==2)printf("| ");
        }
        putchar('\n');
        if(r%3==2)puts("      +-------+-------+-------+");
    }
}
int sudokuComplete(int g[9][9]){int r,c;for(r=0;r<9;r++)for(c=0;c<9;c++)if(g[r][c]==0)return 0;return 1;}
int boardConsistent(int g[9][9]){
    int r,c,n;
    for(r=0;r<9;r++)for(c=0;c<9;c++)if((n=g[r][c])!=0){g[r][c]=0;if(!gridValid(g,r,c,n)){g[r][c]=n;return 0;}g[r][c]=n;}
    return 1;
}
void sudokuCustom(void){
    int g[9][9]={{0}},r,c,fixed[9][9]={{0}};
    screen();puts(GOLD "SUDOKU SOLVER | Enter any Sudoku (0 means empty)." RESET);
    puts("Each row has nine prompts. Invalid starting puzzles are rejected.");
    for(r=0;r<9;r++)for(c=0;c<9;c++){
        printf("Row %d / Column %d [0-9]:\n",r+1,c+1);g[r][c]=input(0,9);
    }
    if(!boardConsistent(g)){puts(RED "Invalid puzzle: duplicate number in a row, column, or box." RESET);pauseGame();return;}
    if(!sudokuSolve(g)){puts(RED "No valid solution exists." RESET);pauseGame();return;}
    puts(CYAN "SOLUTION FOUND USING BACKTRACKING:" RESET);sudokuPrint(g,fixed);pauseGame();
}
void sudokuGame(void){
    int puzzle[9][9],solved[9][9],fixed[9][9],g[9][9],level,holes,action,r,c,v,i,j,hints=0,mistakes=0,score=0;
    time_t started=time(NULL);long duration;
    screen();puts(GOLD "FILE 04 | SUDOKU SECURITY LOCK" RESET);
    puts("University cyber lab has been locked down. Solve the 9x9 grid to decrypt the exit key.");
    puts("Each row, column and 3x3 box must contain 1 to 9 exactly once.");
    puts("Difficulty: 1 Easy | 2 Medium | 3 Hard");level=input(1,3);
    holes=level==1?34:level==2?43:50;
    sudokuGenerate(puzzle,solved,holes);copyGrid(g,puzzle);
    for(i=0;i<9;i++)for(j=0;j<9;j++)fixed[i][j]=puzzle[i][j]!=0;
    for(;;){
        screen();printf(GOLD "SUDOKU LOCK | %s | Hints: %d | Mistakes: %d\n" RESET,level==1?"EASY":level==2?"MEDIUM":"HARD",hints,mistakes);
        sudokuPrint(g,fixed);
        puts("1. Enter or change a digit\n2. Erase a non-fixed digit\n3. Hint (score penalty)\n4. Check progress\n5. Auto-solve (demonstrate algorithm)\n6. Return to suite");
        action=input(1,6);
        if(action==6)return;
        if(action==5){int tmp[9][9];copyGrid(tmp,g);if(boardConsistent(tmp)&&sudokuSolve(tmp)){puts(CYAN "BACKTRACKING SOLVER RESULT:" RESET);sudokuPrint(tmp,fixed);}else puts("Current entries conflict; cannot solve this board.");recordMode("Sudoku Solver Demo",0);pauseGame();return;}
        if(action==4){int correct=0,filled=0;for(i=0;i<9;i++)for(j=0;j<9;j++)if(g[i][j]){filled++;if(g[i][j]==solved[i][j])correct++;}printf("Filled: %d/81 | Matching reference solution: %d | Errors: %d\n",filled,correct,mistakes);pauseGame();continue;}
        if(action==3){
            int empty[81],n=0,spot;for(i=0;i<9;i++)for(j=0;j<9;j++)if(g[i][j]!=solved[i][j])empty[n++]=i*9+j;
            if(!n){puts("Board already solved!");pauseGame();continue;}
            spot=empty[rand()%n];r=spot/9;c=spot%9;g[r][c]=solved[r][c];hints++;
            printf("Hint unlocked: row %d, column %d = %d.\n",r+1,c+1,g[r][c]);pauseGame();
        }else{
            puts("Row (1-9):");r=input(1,9)-1;
            puts("Column (1-9):");c=input(1,9)-1;
            if(fixed[r][c]){puts("Locked cell. Original clues cannot be changed.");pauseGame();continue;}
            if(action==2){g[r][c]=0;puts("Cell erased.");pauseGame();continue;}
            puts("Digit (1-9):");v=input(1,9);
            if(v==solved[r][c]){g[r][c]=v;puts(CYAN "Correct digit." RESET);}
            else{mistakes++;puts(RED "Not consistent with this puzzle's solution. Digit not placed." RESET);}
            pauseGame();
        }
        if(sudokuComplete(g)){
            duration=(long)difftime(time(NULL),started);
            score=250-(int)(duration/10)-hints*15-mistakes*10;
            if(score<10)score=10;
            screen();puts(CYAN "SECURITY LOCK DECRYPTED - SUDOKU SOLVED!" RESET);
            sudokuPrint(g,fixed);
            printf("Time: %ld sec | Hints: %d | Mistakes: %d | Score: %d\n",duration,hints,mistakes,score);
            recordMode("Sudoku Lock",score);pauseGame();return;
        }
    }
}
void sudokuMenu(void){int choice;for(;;){screen();puts(GOLD "FILE 04 | SUDOKU SECURITY LOCK" RESET);puts("1. Play generated Sudoku\n2. Solve your own Sudoku using backtracking\n3. Return to main menu");choice=input(1,3);if(choice==3)return;if(choice==1)sudokuGame();else sudokuCustom();}}


/* FILE 05 - CONNECTED STORY CAMPAIGN
   Original PF concepts: structures, arrays, search, validation, functions,
   random case generator, Sudoku logic, persistent file records and scoring. */
typedef struct {
    int stage, courier, mastermind, decoy, room, eventMinute, clockFast, badge;
    int found[4], attempts, points, moves, fragments[3], digits[3];
    int sequenceStart, sequenceStep, recordedMinute, clockOffset, wordChoice;
    int grid[9][9], original[9][9], solution[9][9], hints, sudokuMistakes;
    int discovered[3], pressVotes[N], lastVote;
} Campaign;
#define CAMPAIGN_SAVE "campaign_save.dat"
const char *campaignRooms[]={"Library annex","Robotics lab","Media studio","Server wing"};
const char *campaignWords[]={"CODES","DOORS","LIGHT"};

void campaignSave(const Campaign *s){
    FILE *f=fopen(CAMPAIGN_SAVE,"wb");
    if(f){fwrite("CZV5",1,4,f);fwrite(s,sizeof *s,1,f);fclose(f);}
    else puts("Warning: cannot save campaign in this folder.");
}
int campaignLoad(Campaign *s){
    char signature[4];FILE *f=fopen(CAMPAIGN_SAVE,"rb");int ok=0;
    if(!f)return 0;
    if(fread(signature,1,4,f)==4 && memcmp(signature,"CZV5",4)==0 &&
       fread(s,sizeof *s,1,f)==1 && s->stage>=1 && s->stage<=5 &&
       s->courier>=0 && s->courier<N && s->mastermind>=0 && s->mastermind<N)ok=1;
    fclose(f);return ok;
}
void campaignNew(Campaign *s){
    int i,order[N],z,used[81]={0};
    memset(s,0,sizeof *s);
    for(i=0;i<N;i++)order[i]=i;
    for(i=N-1;i>0;i--){int k=rand()%(i+1),t=order[i];order[i]=order[k];order[k]=t;}
    s->courier=order[0];s->mastermind=order[1];s->decoy=order[2];
    s->room=rand()%4;s->eventMinute=17+rand()%19;s->clockFast=3+rand()%7;
    s->badge=410+rand()%490;s->stage=1;s->moves=7;s->points=120;
    s->sequenceStart=2+rand()%6;s->sequenceStep=2+rand()%4;
    s->recordedMinute=26+rand()%16;s->clockOffset=2+rand()%6;
    s->wordChoice=rand()%3;
    s->digits[0]=(s->sequenceStart+3*s->sequenceStep)%10;
    s->digits[1]=(s->recordedMinute-s->clockOffset)%10;
    s->digits[2]=s->wordChoice+4;
    sudokuGenerate(s->grid,s->solution,0);
    /* Only seven blank squares in campaign: playable without excessive time. */
    for(i=0;i<7;i++){
        do{z=rand()%81;}while(used[z]);used[z]=1;
        s->grid[z/9][z%9]=0;
    }
    copyGrid(s->original,s->grid);
    campaignSave(s);
}
void campaignHeader(Campaign *s,const char *title){
    screen();printf(GOLD "CAMPAIGN | EPISODE %d/4: %s\n" RESET,s->stage>4?4:s->stage,title);
    printf("Investigation points: %d | Wrong deductions: %d | Auto-save enabled\n",s->points,s->attempts);
    puts("------------------------------------------------------------------");
}
int campaignPenalty(Campaign *s,int cost){
    s->points-=cost;if(s->points<0)s->points=0;s->attempts++;campaignSave(s);
    if(s->attempts>=10){puts(RED "Too many failed deductions. Case archived as unresolved." RESET);recordMode("Campaign - Unresolved",s->points);remove(CAMPAIGN_SAVE);pauseGame();return 0;}
    printf(RED "Incorrect. -%d points. Total wrong deductions: %d/10\n" RESET,cost,s->attempts);
    return 0;
}
int campaignEpisode1(Campaign *s){
    int choice,i,claimant,t;
    for(;;){
        campaignHeader(s,"THE VANISHED HARD DRIVE");
        puts("Karachi. University media lab. 11:43 PM. A confidential drive disappears.");
        puts("An anonymous message blames someone. Evidence may contradict it.");
        printf("Evidence recovered %d/4 | Remaining investigation actions: %d\n",s->found[0]+s->found[1]+s->found[2]+s->found[3],s->moves);
        puts("1. Crime scene report\n2. CCTV timestamp\n3. Security badge register\n4. Interview witnesses\n5. Open evidence board\n6. Identify the courier\n7. Save and main menu");
        choice=input(1,7);if(choice==7){campaignSave(s);return 0;}
        if(choice>=1&&choice<=4){
            if(!s->found[choice-1]){
                if(s->moves<=0){puts("Investigation time exhausted. Review what you have.");pauseGame();continue;}
                s->found[choice-1]=1;s->moves--;s->points+=5;campaignSave(s);
            }
            if(choice==1)printf("SCENE REPORT: Drive stolen from %s. Door reader cached badge %03d.\n",campaignRooms[s->room],s->badge);
            if(choice==2)printf("CCTV: screen shows 23:%02d. Recorder is %d minutes FAST.\n",s->eventMinute+s->clockFast,s->clockFast);
            if(choice==3){
                puts("BADGE REGISTER: Shared badges exist (badge alone is not proof).");
                for(i=0;i<N;i++){
                    int card=i==s->courier||i==s->decoy?s->badge:500+i*9;
                    printf("%-9s: badge %03d\n",p[i].name,card);
                }
            }
            if(choice==4){
                puts("VERIFIED WITNESS + GATE LOG (three clues must agree):");
                for(i=0;i<N;i++){
                    int room=i==s->courier||i==s->decoy?s->room:(s->room+1+i%3)%4;
                    int minute=i==s->courier?s->eventMinute:s->eventMinute+(i%2?2:4);
                    printf("%-9s | %-15s | 23:%02d\n",p[i].name,campaignRooms[room],minute);
                }
            }
            pauseGame();continue;
        }
        if(choice==5){
            puts("EVIDENCE BOARD:");
            if(s->found[0])printf("LOCATION: %s | cached badge %d\n",campaignRooms[s->room],s->badge);
            if(s->found[1])printf("CCTV corrected minute: %02d (subtract %d from %02d)\n",s->eventMinute,s->clockFast,s->eventMinute+s->clockFast);
            if(s->found[2])puts("Badge records collected.");
            if(s->found[3])puts("Verified location/time records collected.");
            claimant=s->decoy;
            printf("UNVERIFIED anonymous chat: 'Maine %s ko bhaagtay dekha!'\n",p[claimant].name);
            puts("The chat is an allegation, NOT forensic proof.");pauseGame();continue;
        }
        if(!s->found[0]||!s->found[1]||!s->found[2]||!s->found[3]){
            puts("Need scene, CCTV, badge, and witness records before accusation.");pauseGame();continue;
        }
        puts("Who physically carried the drive? (The courier is NOT necessarily the mastermind.)");
        for(i=0;i<N;i++)printf("%d. %s\n",i+1,p[i].name);
        claimant=input(1,N)-1;
        puts("Enter corrected CCTV minute (0-59):");t=input(0,59);
        if(claimant==s->courier&&t==s->eventMinute){
            puts(CYAN "EVIDENCE CHAIN VERIFIED. Courier located. A basement key is recovered." RESET);
            puts("But a secret payment suggests someone else arranged the theft.");
            s->stage=2;s->points+=50;campaignSave(s);pauseGame();return 1;
        }
        if(campaignPenalty(s,12))return -1;
        pauseGame();
    }
}
int campaignEpisode2(Campaign *s){
    int option,guess,i,sw[3];
    for(;;){
        campaignHeader(s,"BASEMENT 404: EMERGENCY LOCKDOWN");
        printf("Recovered courier: %s. Three encrypted terminals guard the evidence vault.\n",p[s->courier].name);
        printf("Unlocked fragments: %d/3\n",s->fragments[0]+s->fragments[1]+s->fragments[2]);
        puts("1. Restore power + sequence puzzle\n2. Correct security recorder time\n3. Decode radio cipher\n4. Review code fragments\n5. Enter vault code\n6. Save and main menu");
        option=input(1,6);if(option==6){campaignSave(s);return 0;}
        if(option>=1&&option<=3&&s->fragments[option-1]){printf("Already unlocked: fragment %d = %d\n",option,s->digits[option-1]);pauseGame();continue;}
        if(option==1){
            puts("Electrical note: first and third switches ON, second switch OFF.");
            for(i=0;i<3;i++){printf("Switch %d (0 OFF / 1 ON):\n",i+1);sw[i]=input(0,1);}
            if(sw[0]&&sw[1]==0&&sw[2]){
                printf("Sequence: %d, %d, %d, ?\n",s->sequenceStart,s->sequenceStart+s->sequenceStep,s->sequenceStart+2*s->sequenceStep);
                guess=input(0,99);
                if(guess==s->sequenceStart+3*s->sequenceStep)s->fragments[0]=1;
                else if(campaignPenalty(s,8))return -1;
            }else if(campaignPenalty(s,8))return -1;
        }
        if(option==2){
            printf("Recorder says 22:%02d and runs %d minutes FAST. Actual minute?\n",s->recordedMinute,s->clockOffset);
            guess=input(0,59);
            if(guess==s->recordedMinute-s->clockOffset)s->fragments[1]=1;
            else if(campaignPenalty(s,8))return -1;
        }
        if(option==3){
            const char *word=campaignWords[s->wordChoice];
            printf("Encrypted radio: ");for(i=0;i<5;i++)putchar(word[i]=='Z'?'A':word[i]+1);puts("");
            puts("Each character shifted +1. Decode by shifting BACK.");
            puts("1 CODES | 2 DOORS | 3 LIGHT");guess=input(1,3);
            if(guess==s->wordChoice+1)s->fragments[2]=1;
            else if(campaignPenalty(s,8))return -1;
        }
        if(option==4){for(i=0;i<3;i++){if(s->fragments[i])printf("Fragment %d: %d\n",i+1,s->digits[i]);else printf("Fragment %d: LOCKED\n",i+1);}}
        if(option==5){
            if(!s->fragments[0]||!s->fragments[1]||!s->fragments[2])puts("Solve all three terminals first.");
            else{
                puts("Enter 3-digit vault code:");guess=input(0,999);
                if(guess==100*s->digits[0]+10*s->digits[1]+s->digits[2]){
                    puts(CYAN "VAULT OPEN! A scrambled 9x9 security archive is recovered." RESET);
                    s->stage=3;s->points+=60;campaignSave(s);pauseGame();return 1;
                }
                if(campaignPenalty(s,10))return -1;
            }
        }
        campaignSave(s);pauseGame();
    }
}
int campaignEpisode3(Campaign *s){
    int action,row,col,value,i,j,solved=1;
    for(;;){
        campaignHeader(s,"SUDOKU: DECRYPT THE ARCHIVE");
        puts("To access the security recording, restore seven missing digits.");
        printf("Hints used: %d | Wrong entries: %d\n",s->hints,s->sudokuMistakes);
        sudokuPrint(s->grid,s->original);
        puts("1. Enter digit\n2. Get hint (-12 points)\n3. Check progress\n4. Save and main menu");
        action=input(1,4);if(action==4){campaignSave(s);return 0;}
        if(action==1){
            puts("Row (1-9):");row=input(1,9)-1;puts("Column (1-9):");col=input(1,9)-1;
            if(s->original[row][col]){puts("Locked clue: cannot edit the given number.");pauseGame();continue;}
            puts("Digit (1-9):");value=input(1,9);
            if(value==s->solution[row][col]){s->grid[row][col]=value;puts(CYAN "Correct. Data block restored." RESET);}
            else{s->sudokuMistakes++;if(campaignPenalty(s,5))return -1;}
        }
        if(action==2){
            int empty[81],n=0,z;
            for(i=0;i<9;i++)for(j=0;j<9;j++)if(s->grid[i][j]==0)empty[n++]=i*9+j;
            if(!n)puts("No missing cells remain.");
            else{z=empty[rand()%n];row=z/9;col=z%9;s->grid[row][col]=s->solution[row][col];s->hints++;s->points-=12;if(s->points<0)s->points=0;printf("Security hint: R%d C%d = %d\n",row+1,col+1,s->grid[row][col]);}
        }
        if(action==3){int left=0;for(i=0;i<9;i++)for(j=0;j<9;j++)if(!s->grid[i][j])left++;printf("Missing cells remaining: %d\n",left);}
        solved=1;for(i=0;i<9;i++)for(j=0;j<9;j++)if(s->grid[i][j]!=s->solution[i][j])solved=0;
        if(solved){
            puts(CYAN "ARCHIVE RESTORED. Encrypted order identifies a second person." RESET);
            printf("Courier %s was hired, but the instruction originated from the server wing.\n",p[s->courier].name);
            puts("The final evidence ledger can now be accessed. Find WHO issued the order.");
            s->stage=4;s->points+=80;campaignSave(s);pauseGame();return 1;
        }
        campaignSave(s);pauseGame();
    }
}
int campaignEpisode4(Campaign *s){
    int action,i,guess,most=-1,top=0,level;
    const int masterMinute=37;
    for(;;){
        campaignHeader(s,"MAFIA PROTOCOL: THE FINAL REVEAL");
        puts("A campus committee assembles. The courier insists they were paid.");
        puts("The mastermind is still among the eight characters.");
        puts("1. Decrypt server access log\n2. Compare payment trail\n3. Audit CCTV alibis\n4. Public vote and final accusation\n5. Save and main menu");
        action=input(1,5);if(action==5){campaignSave(s);return 0;}
        if(action==1){
            s->discovered[0]=1;
            puts("SERVER ACCESS LOG | ENCRYPTED KEYWORD: 'NIGHT' (decrypted)");
            puts("At 23:37, a senior account issued the remote wipe command.");
            for(i=0;i<N;i++)printf("%-9s login 23:%02d | access %s\n",p[i].name,i==s->mastermind||i==s->decoy?masterMinute:20+i,i==s->mastermind||i==s->decoy?"senior":"basic");
        }
        if(action==2){
            s->discovered[1]=1;
            printf("PAYMENT TRAIL: %s received payment at 23:37.\n",p[s->courier].name);
            puts("The payee's receipt shows the sender's account group:");
            for(i=0;i<N;i++)printf("%-9s account group: %s\n",p[i].name,i==s->mastermind||i==s->courier?"X7":i==s->decoy?"Q2":"L1");
            puts("IMPORTANT: Courier account isn't proof of ordering the theft.");
        }
        if(action==3){
            s->discovered[2]=1;
            puts("CCTV + witness audit, at exact command time 23:37:");
            for(i=0;i<N;i++)printf("%-9s | %s\n",p[i].name,i==s->mastermind?"Server wing / no verified alibi":i==s->decoy?"Lab / verified witness":i==s->courier?"Parking / verified witness":"Campus gate / verified witness");
            puts("Combine login, payment account and unverified presence.");
        }
        if(action==4){
            if(!s->discovered[0]||!s->discovered[1]||!s->discovered[2]){
                puts("The committee demands all three authenticated records.");pauseGame();continue;
            }
            puts("WHO ORDERED THE THEFT? Choose accused ID:");
            for(i=0;i<N;i++)printf("%d. %s\n",i+1,p[i].name);
            guess=input(1,N)-1;
            puts("Which method validates the conclusion?\n1. Anonymous rumor\n2. Combined access + payment + alibi\n3. Personal intuition");level=input(1,3);
            if(guess==s->mastermind&&level==2){
                int v;
                /* Public vote: witnesses trust the three verified records,
                   and one random voter may abstain instead of following proof. */
                for(i=0;i<N;i++){
                    v= i==s->mastermind?s->courier:(rand()%5==0?s->decoy:s->mastermind);
                    s->pressVotes[v]++;
                    printf("%-9s votes for %-9s\n",p[i].name,p[v].name);
                }
                for(i=0;i<N;i++)if(s->pressVotes[i]>top){top=s->pressVotes[i];most=i;}
                
                printf("Public highest vote: %s (%d votes).\n",p[most].name,top);
                puts(CYAN "CASE CLOSED: the mastermind has been exposed by verified evidence!" RESET);
                printf("Courier: %s | Mastermind: %s | Final score: %d\n",p[s->courier].name,p[s->mastermind].name,s->points+100);
                recordMode("Connected Campaign - Solved",s->points+100);
                remove(CAMPAIGN_SAVE);pauseGame();return 0;
            }
            puts("Committee rejects the accusation: it contradicts the verified chain.");
            if(campaignPenalty(s,20))return -1;
        }
        campaignSave(s);pauseGame();
    }
}
void campaign(void){
    Campaign state;int option,rc;
    for(;;){
        screen();puts(CYAN "FILE 05 | THE BLACKOUT CONSPIRACY (CONNECTED CAMPAIGN)" RESET);
        puts("1. Start new story\n2. Continue saved story\n3. How the campaign works\n4. Return to game suite");
        option=input(1,4);if(option==4)return;
        if(option==3){
            puts("EP 1: Find the courier using four cross-checked clues.");
            puts("EP 2: Solve three linked escape room challenges to open the vault.");
            puts("EP 3: Restore a 9x9 Sudoku archive (seven missing digits).");
            puts("EP 4: Cross-check access, payment and alibi logs to expose mastermind.");
            puts("Every chapter saves progress. Ten wrong deductions end the case.");
            pauseGame();continue;
        }
        if(option==1){
            puts("A new story replaces your previous campaign save. Proceed? 1 Yes / 2 No");
            if(input(1,2)==2)continue;
            campaignNew(&state);
        }else if(!campaignLoad(&state)){puts("No compatible campaign save found. Start a new story.");pauseGame();continue;}
        for(;;){
            rc=state.stage==1?campaignEpisode1(&state):state.stage==2?campaignEpisode2(&state):state.stage==3?campaignEpisode3(&state):campaignEpisode4(&state);
            if(rc!=1)break;
        }
    }
}

#include "detective_cases.h"

int main(void){
    int opt;
    enableTerminal();
#ifndef CASE_GUI
startupClock();
#endif
gameStarted=time(NULL);
#ifndef CASE_GUI
clockEnabled=1;
#endif
setvbuf(stdout,NULL,_IONBF,0);srand((unsigned)time(NULL));
    for(;;){
        screen();
        puts(CYAN "THE MYSTERY FILES | V5.0 - STORY CAMPAIGN EDITION\n" RESET
             "1. THE BLACKOUT CONSPIRACY (Connected Story Campaign)\n"
             "2. Mafia: Trust No One\n"
             "3. The Midnight Murder\n"
             "4. Escape the Darkness\n"
             "5. Sudoku Security Lock\n"
             "6. Case history\n"
             "7. Detective Case Files (Proposal Edition)\n"
             "8. Exit");
        opt=input(1,8);
        if(opt==8)return 0;
        if(opt==1)campaign();
        if(opt==2)mafia();
        if(opt==3)murder();
        if(opt==4)escapeRoom();
        if(opt==5)sudokuMenu();
        if(opt==7)detectiveCases();
        if(opt==6){
            char line[200];FILE *f=fopen("case_history.txt","r");
            if(f){while(fgets(line,sizeof line,f))printf("%s",line);fclose(f);}
            else puts("No completed cases.");
            pauseGame();
        }
    }
}
