/* CASE ZERO - Detective Case Files (C11 / beginner-friendly functions).
   Include from case_zero.c. Uses existing input(), pauseGame(), screen(), p[].
   One clue on its own never proves guilt: location, access and alibi must agree. */
#ifndef CASE_ZERO_DETECTIVE_CASES_H
#define CASE_ZERO_DETECTIVE_CASES_H
#define DC_SAVE "detective_save.dat"
#define DC_SCORES "detective_scores.txt"
#define DC_HISTORY "detective_history.txt"
#define DC_COUNT 7

typedef struct {
    char detective[40];
    int difficulty, caseType, culprit, victim, room, minute, fast, badge;
    int moves, initialMoves, found[5], suspicion[N], hints, solved, actions;
} DetectiveCase;

const char *dcCrimes[] = {"Missing research drive", "The midnight murder", "Stolen prototype", "Server room breach"};
const char *dcRooms[] = {"Library", "Computer lab", "Media room", "Server wing"};
const char *dcRank(int score){
    if(score>=140)return "Master Detective";
    if(score>=105)return "Senior Detective";
    if(score>=70)return "Inspector";
    return "Rookie";
}
int dcExpectedRoom(const DetectiveCase *s,int id){
    if(id==s->culprit||id==(s->culprit+2)%N||id==(s->culprit+4)%N)return s->room;
    return (s->room+1+id%3)%4;
}
int dcExpectedBadge(const DetectiveCase *s,int id){
    if(id==s->culprit||id==(s->culprit+2)%N||id==(s->culprit+6)%N)return s->badge;
    return 100+id;
}
int dcExpectedMinute(const DetectiveCase *s,int id){
    if(id==s->culprit||id==(s->culprit+4)%N||id==(s->culprit+6)%N)return s->minute;
    return s->minute+2+id;
}
void dcNew(DetectiveCase *s,const char *name,int level){
    int i;
    memset(s,0,sizeof(*s));
    snprintf(s->detective,sizeof(s->detective),"%s",name);
    s->difficulty=level;
    s->caseType=rand()%4;s->culprit=rand()%N;
    s->victim=(s->culprit+1+rand()%(N-1))%N;
    s->room=rand()%4;s->minute=10+rand()%35;s->fast=3+rand()%8;
    s->badge=400+rand()%500;
    s->moves=s->initialMoves=(level==1?10:level==2?8:6);
    for(i=0;i<N;i++)s->suspicion[i]=0;
}
int dcSave(const DetectiveCase *s){
    FILE *f=fopen(DC_SAVE,"wb");
    if(!f)return 0;
    if(fwrite("CZDC1",1,5,f)!=5 || fwrite(s,sizeof(*s),1,f)!=1){fclose(f);return 0;}
    fclose(f);return 1;
}
int dcLoad(DetectiveCase *s){
    char mark[5];FILE *f=fopen(DC_SAVE,"rb");int ok=0;
    if(!f)return 0;
    if(fread(mark,1,5,f)==5 && memcmp(mark,"CZDC1",5)==0 && fread(s,sizeof(*s),1,f)==1 &&
       s->difficulty>=1&&s->difficulty<=3 && s->culprit>=0&&s->culprit<N &&
       s->room>=0&&s->room<4 && s->minute>=10&&s->minute<45 &&
       s->moves>=0&&s->moves<=10 && s->initialMoves>=6&&s->initialMoves<=10 &&
       s->detective[0])ok=1;
    fclose(f);return ok;
}
void dcInfo(const DetectiveCase *s){
    int i;
    printf("\nCASE: %s | Detective: %s | Difficulty: %s\n",dcCrimes[s->caseType],s->detective,
           s->difficulty==1?"Easy":s->difficulty==2?"Medium":"Hard");
    printf("Moves left: %d/%d | Evidence: ",s->moves,s->initialMoves);
    for(i=0;i<5;i++)printf("%c",s->found[i]?'#':'-');
    puts("");
}
void dcBoard(const DetectiveCase *s){
    int i;
    puts("\nEVIDENCE INVENTORY:  [X = collected / - = missing]");
    printf("[%c] Scene | [%c] CCTV | [%c] Access log | [%c] Interviews | [%c] Fingerprint\n",
           s->found[0]?'X':'-',s->found[1]?'X':'-',s->found[2]?'X':'-',s->found[3]?'X':'-',s->found[4]?'X':'-');
    puts("SUSPECT ANALYSIS: Suspicions are leads, not proof.");
    for(i=0;i<N;i++)if(i!=s->victim){
        int j;
        printf("%d. %-10s  %2d/10  ",i+1,p[i].name,s->suspicion[i]);
        for(j=0;j<s->suspicion[i];j++)putchar('#');
        putchar('\n');
    }
    puts("You must link location, corrected CCTV minute, and matching access badge.");
}
void dcRaise(DetectiveCase *s,int i,int points){
    s->suspicion[i]+=points;
    if(s->suspicion[i]>10)s->suspicion[i]=10;
}
void dcInvestigate(DetectiveCase *s,int option){
    int i;
    if(s->found[option-1]){puts("Already collected! Review the evidence board for free.");return;}
    if(s->moves==0){puts("Warrant expired. No investigation moves remain.");return;}
    s->moves--;s->actions++;s->found[option-1]=1;
    switch(option){
    case 1:
        printf("SCENE REPORT: Incident location: %s. Access reader stored suffix %d.\n",
               dcRooms[s->room],s->badge);
        puts("The recorder timestamp may be incorrect. Find independent records.");
        dcRaise(s,s->culprit,1);break;
    case 2:
        printf("RECOVERED CCTV: Recording stamp 11:%02d PM. Clock was %d minutes FAST.\n",
               s->minute+s->fast,s->fast);
        puts("Subtract the offset to reconstruct the incident minute.");
        dcRaise(s,s->culprit,1);break;
    case 3:
        puts("ACCESS LOG: multiple people can share a card. Do not rely on badge alone.");
        for(i=0;i<N;i++)if(i!=s->victim)printf("%-10s  badge %d\n",p[i].name,dcExpectedBadge(s,i));
        dcRaise(s,s->culprit,2);dcRaise(s,(s->culprit+2)%N,1);break;
    case 4:
        puts("INTERVIEWS: locations and timestamps verified independently.");
        for(i=0;i<N;i++)if(i!=s->victim)printf("%-10s  %-12s  11:%02d PM  %s\n",p[i].name,
                 dcRooms[dcExpectedRoom(s,i)],dcExpectedMinute(s,i),
                 i%2?"Gate witness":"Signed lab register");
        dcRaise(s,s->culprit,3);break;
    case 5:
        puts("FINGERPRINT / ACCESS CHECK: Security seals point to badge holders.");
        printf("Recovered trace: badge suffix %d; the trace itself is not exclusive.\n",s->badge);
        dcRaise(s,s->culprit,2);break;
    }
    if(!dcSave(s))puts("Warning: save failed. Check folder permissions.");
}
void dcReview(const DetectiveCase *s){
    dcBoard(s);
    if(s->found[0])printf("SCENE: %s, badge %d\n",dcRooms[s->room],s->badge);
    if(s->found[1])printf("CCTV: 11:%02d PM, clock %d minutes fast\n",s->minute+s->fast,s->fast);
    if(s->found[2]){int i;for(i=0;i<N;i++)if(i!=s->victim)
       printf("Badge %-10s : %d\n",p[i].name,dcExpectedBadge(s,i));}
    if(s->found[3]){int i;for(i=0;i<N;i++)if(i!=s->victim)
       printf("Alibi %-10s : %s, 11:%02d PM\n",p[i].name,dcRooms[dcExpectedRoom(s,i)],dcExpectedMinute(s,i));}
    if(s->found[4])puts("Access trace: partial fingerprint, supports the badge but not identity alone.");
}
void dcLeaderboard(void){
    typedef struct {char name[40];int score;} Result;
    Result top[100],temp;int n=0,i,j;FILE *f=fopen(DC_SCORES,"r");
    if(!f){puts("Leaderboard is empty.");return;}
    while(n<100 && fscanf(f,"%39[^|]|%d\n",top[n].name,&top[n].score)==2)n++;
    fclose(f);
    for(i=0;i<n;i++)for(j=i+1;j<n;j++)if(top[j].score>top[i].score){temp=top[i];top[i]=top[j];top[j]=temp;}
    puts("\nTOP DETECTIVES | Scores sorted highest to lowest");
    for(i=0;i<n&&i<10;i++)printf("%2d. %-25s  %3d  %s\n",i+1,top[i].name,top[i].score,dcRank(top[i].score));
    if(n==0)puts("No recorded scores yet.");
}
void dcFinish(DetectiveCase *s,int suspect,int proof,int minute){
    int solved=(suspect==s->culprit && proof==3 && minute==s->minute &&
                s->found[0] && s->found[1] && s->found[2] && s->found[3]);
    int collected=0,i,score;
    FILE *f;
    for(i=0;i<5;i++)collected+=s->found[i];
    score=solved?60+collected*10+s->moves*4-s->hints*12:collected*3-s->hints*8;
    if(score<0)score=0;
    printf("\n%s\n",solved?"CASE SOLVED! Complete evidence chain accepted.":
           "CASE INCOMPLETE / WRONG ARREST. Evidence was insufficient.");
    printf("Your score: %d | Rank: %s | Moves left: %d\n",score,dcRank(score),s->moves);
    if(solved)puts("Excellent deduction. Every independent source supports your conclusion.");
    else puts("The suspect has NOT been publicly identified. Replay and collect stronger evidence.");
    f=fopen(DC_SCORES,"a");if(f){fprintf(f,"%s|%d\n",s->detective,score);fclose(f);}
    f=fopen(DC_HISTORY,"a");if(f){fprintf(f,"%s | %s | %s | score %d\n",s->detective,
          dcCrimes[s->caseType],solved?"SOLVED":"FAILED",score);fclose(f);}
    remove(DC_SAVE);
    pauseGame();
}
void dcPlay(DetectiveCase *s){
    int choice,i,suspect,proof,minute;
    for(;;){
        screen();puts(GOLD "DETECTIVE CASE FILES | RANDOMIZED INVESTIGATION" RESET);
        dcInfo(s);
        printf("Victim / affected party: %s | Incident occurred at 11:XX PM.\n",p[s->victim].name);
        puts("1. Inspect crime scene\n2. Recover CCTV\n3. Check access cards\n4. Interview suspects / verify alibis\n5. Fingerprint check");
        puts("6. Evidence board & suspicion meter\n7. Request hint (score penalty)\n8. Final accusation\n9. Save & return to suite");
        choice=input(1,9);
        if(choice<=5){dcInvestigate(s,choice);pauseGame();}
        if(choice==6){dcReview(s);pauseGame();}
        if(choice==7){s->hints++;printf("HINT: %s\n",!s->found[1]?"CCTV is needed to correct the timeline.":
            !s->found[2]?"Compare shared badge numbers in the access log.":
            !s->found[3]?"Interviews can verify locations and alibis.":
            "Match location + time + badge. A single clue is insufficient.");dcSave(s);pauseGame();}
        if(choice==8){
            puts("WHO DO YOU ACCUSE? (choose the original ID)");
            for(i=0;i<N;i++)if(i!=s->victim)printf("%d. %s\n",i+1,p[i].name);
            do{suspect=input(1,N)-1;if(suspect==s->victim)puts("That person is the victim, choose another.");}while(suspect==s->victim);
            puts("CHOOSE PROOF: 1 Anonymous rumor | 2 Access badge alone | 3 Location + corrected time + badge | 4 Suspicion only");
            proof=input(1,4);
            puts("Enter corrected CCTV MINUTE (0-59); for example 11:13 PM -> 13:");
            minute=input(0,59);
            dcFinish(s,suspect,proof,minute);return;
        }
        if(choice==9){dcSave(s);return;}
    }
}
void detectiveCases(void){
    DetectiveCase s;int ch,level;char name[40];
    for(;;){
        screen();puts(GOLD "DETECTIVE CASE FILES | PROPOSAL EDITION" RESET);
        puts("1. New randomized investigation\n2. Continue saved investigation\n3. Leaderboard\n4. Case history\n5. Return to main menu");
        ch=input(1,5);
        if(ch==5)return;
        if(ch==1){
            printf("Detective name (press Enter for Detective): ");fflush(stdout);
#ifdef CASE_GUI
            puts("@@CZ_TEXT@@");fflush(stdout);
#endif
            readLine(name,sizeof(name));if(name[0]==0)strcpy(name,"Detective");
            /* Strip file delimiters so score records stay readable. */
            for(int i=0;name[i];i++)if(name[i]=='|'||name[i]=='\n')name[i]=' ';
            puts("Choose difficulty: 1 Easy | 2 Medium | 3 Hard");level=input(1,3);
            dcNew(&s,name,level);dcSave(&s);dcPlay(&s);
        }
        if(ch==2){if(dcLoad(&s))dcPlay(&s);else {puts("No valid saved investigation found.");pauseGame();}}
        if(ch==3){dcLeaderboard();pauseGame();}
        if(ch==4){char line[240];FILE *f=fopen(DC_HISTORY,"r");
            if(f){puts("CASE HISTORY:");while(fgets(line,sizeof(line),f))fputs(line,stdout);fclose(f);}
            else {puts("No case history yet.");}pauseGame();}
    }
}
#endif
