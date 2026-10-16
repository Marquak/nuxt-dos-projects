/* 3D Checkers 88. Original compact English/American checkers engine.
 * Research and rule sources are recorded in CREDITS.TXT. No external engine
 * code is incorporated. A move here is ONE leg; a capture continuation keeps
 * the same side and the search depth until the complete turn has ended. */
#ifndef CHECKERS_ENGINE_H
#define CHECKERS_ENGINE_H
#include <string.h>
#include <stdlib.h>
#define MAXMOVES 48
#define MAXPLY 28
#define WINVALUE 15000
typedef signed char S8;
typedef unsigned char U8;
typedef struct { S8 cell[32]; S8 side, forced; U8 quiet; } Board;
typedef struct { U8 from, to; S8 over; } Move;
static S8 nextsq[32][4], jumpsq[32][4];
static int rowof(int s) { return s >> 2; }
static int colof(int s) { return ((s & 3) << 1) + ((rowof(s) + 1) & 1); }
static int square(int r, int c) {
    if (r < 0 || r > 7 || c < 0 || c > 7 || !((r + c) & 1)) return -1;
    return r * 4 + c / 2;
}
static void engine_init(void) {
    static const int dr[4] = {-1,-1,1,1}, dc[4] = {-1,1,-1,1};
    int s,d;
    for(s=0;s<32;++s) for(d=0;d<4;++d) {
        nextsq[s][d]=(S8)square(rowof(s)+dr[d],colof(s)+dc[d]);
        jumpsq[s][d]=(S8)square(rowof(s)+dr[d]*2,colof(s)+dc[d]*2);
    }
}
static void board_start(Board *b) {
    int s;
    memset(b,0,sizeof(*b)); b->side=1; b->forced=-1;
    for(s=0;s<12;++s) b->cell[s]=-1;
    for(s=20;s<32;++s) b->cell[s]=1;
}
static int owns(int p,int side) { return p && ((p>0)==(side>0)); }
static int directions(int p,int d) { return p==2 || p==-2 || (p>0 ? d<2 : d>=2); }
static int can_jump(const Board *b,int s) {
    int d,n,t,p=b->cell[s];
    if(!p) return 0;
    for(d=0;d<4;++d) if(directions(p,d)) {
        n=nextsq[s][d]; t=jumpsq[s][d];
        if(n>=0 && t>=0 && !b->cell[t] && owns(b->cell[n],-p)) return 1;
    }
    return 0;
}
static int moves(const Board *b,Move *out) {
    int s,d,n,t,count=0,p;
    for(s=0;s<32;++s) if(owns(b->cell[s],b->side) && (b->forced<0 || s==b->forced)) {
        p=b->cell[s];
        for(d=0;d<4;++d) if(directions(p,d)) {
            n=nextsq[s][d]; t=jumpsq[s][d];
            if(n>=0 && t>=0 && !b->cell[t] && owns(b->cell[n],-p)) {
                out[count].from=(U8)s; out[count].to=(U8)t; out[count++].over=(S8)n;
            }
        }
    }
    if(count || b->forced>=0) return count;
    for(s=0;s<32;++s) if(owns(b->cell[s],b->side)) {
        p=b->cell[s];
        for(d=0;d<4;++d) if(directions(p,d)) {
            t=nextsq[s][d];
            if(t>=0 && !b->cell[t]) {
                out[count].from=(U8)s; out[count].to=(U8)t; out[count++].over=-1;
            }
        }
    }
    return count;
}
/* Return 1 when the turn ended. Legal legs only. */
static int apply(Board *b,Move m) {
    int p=b->cell[m.from],crown=0;
    b->cell[m.from]=0;
    if(m.over>=0) b->cell[m.over]=0;
    if((p==1 && rowof(m.to)==0) || (p==-1 && rowof(m.to)==7)) {
        p*=2; crown=1;
    }
    b->cell[m.to]=(S8)p;
    if(m.over>=0 || p==1 || p==-1 || crown) b->quiet=0;
    if(m.over>=0 && !crown && can_jump(b,m.to)) {
        b->forced=(S8)m.to; return 0;
    }
    if(m.over<0 && (p==2 || p==-2) && !crown && b->quiet<80) ++b->quiet;
    b->forced=-1; b->side=(S8)-b->side; return 1;
}
static int evaluate(const Board *b) {
    int s,p,r,c,v,total=0;
    for(s=0;s<32;++s) if((p=b->cell[s])!=0) {
        r=rowof(s); c=colof(s);
        if(p==2 || p==-2) v=180+(3-abs(3-r))*2+(3-abs(3-c))*2;
        else {
            int advance=p>0?7-r:r;
            v=100+advance*3;
            if(!advance) v+=7; /* Keep a useful back-rank guard. */
            if(r>1 && r<6 && c>1 && c<6) v+=5;
        }
        total+=p>0?v:-v;
    }
    return total*b->side;
}
/* Full positions, not hashes, make the actual threefold rule collision-free.
 * Irreversible moves clear this history; 81 is enough before the 40-move draw. */
static Board history[81];
static int histories;
static void history_reset(const Board *b) { history[0]=*b; histories=1; }
static void history_add(const Board *b) {
    if(!b->quiet) histories=0;
    if(histories==81) { memmove(history,history+1,80*sizeof(Board)); --histories; }
    history[histories++]=*b;
}
static int samepos(const Board *a,const Board *b) {
    return a->side==b->side && a->forced==b->forced && !memcmp(a->cell,b->cell,32);
}
static int repeated(const Board *b) {
    int i,n=0;
    for(i=0;i<histories;++i) if(samepos(history+i,b) && ++n>=3) return 1;
    return 0;
}
typedef int (*SearchPoll)(void);
static Board searchboard[MAXPLY];
static Move searchmoves[MAXPLY][MAXMOVES];
static unsigned long nodes,nodecap;
static int searchstop,completed_depth;
static SearchPoll searchpoll;
static int priority(const Board *b,Move m) {
    int p=b->cell[m.from],v=0;
    if(m.over>=0) v=100+(abs(b->cell[m.over])==2?40:0);
    if((p==1 && rowof(m.to)==0)||(p==-1 && rowof(m.to)==7)) v+=80;
    return v+(3-abs(3-colof(m.to)));
}
static int search(int ply,int depth,int alpha,int beta) {
    Board *b=searchboard+ply;
    Move *list=searchmoves[ply],tmp;
    int n,i,j,v,best=-WINVALUE,turn;
    ++nodes;
    if(nodes>=nodecap || ((nodes&31)==0 && searchpoll && searchpoll())) { searchstop=1; return 0; }
    n=moves(b,list);
    if(!n) return -WINVALUE+ply;
    if(b->quiet>=80) return 0;
    /* Also avoid loops within this search. Actual match history is exact above. */
    if(b->forced<0) {
        for(i=0;i<ply;++i) if(samepos(b,searchboard+i)) return 0;
        if(b->quiet) {for(i=0,j=0;i<histories;++i) if(samepos(b,history+i) && ++j>=2) return 0;}
    }
    if(ply==MAXPLY-1 || (depth<=0 && list[0].over<0)) return evaluate(b);
    for(i=1;i<n;++i) {
        tmp=list[i]; j=i;
        while(j && priority(b,tmp)>priority(b,list[j-1])) { list[j]=list[j-1]; --j; }
        list[j]=tmp;
    }
    for(i=0;i<n;++i) {
        searchboard[ply+1]=*b;
        turn=apply(searchboard+ply+1,list[i]);
        if(turn) v=-search(ply+1,depth-1,-beta,-alpha);
        else v=search(ply+1,depth,alpha,beta);
        if(searchstop) return 0;
        if(v>best) best=v;
        if(v>alpha) alpha=v;
        if(alpha>=beta) break;
    }
    return best;
}
/* Iterative deepening retains only fully completed root iterations. */
static Move computer(const Board *b,int level,unsigned long budget,SearchPoll poll) {
    static const int depths[3]={2,5,8};
    Move root[MAXMOVES],tmp,best;
    int n=moves(b,root),d,i,v,score,index=0,turn;
    nodes=0; nodecap=budget; searchstop=0; searchpoll=poll; completed_depth=0;
    memset(&best,0,sizeof(best)); best.over=-1;
    if(!n) return best;
    best=root[0]; searchboard[0]=*b;
    for(d=1;d<=depths[level];++d) {
        score=-WINVALUE-1; index=0;
        for(i=0;i<n;++i) {
            searchboard[1]=*b; turn=apply(searchboard+1,root[i]);
            v=turn?-search(1,d-1,-WINVALUE,-score):search(1,d,score,WINVALUE);
            if(searchstop) break;
            if(v>score) { score=v; index=i; }
        }
        if(searchstop) break;
        best=root[index]; completed_depth=d;
        tmp=root[0]; root[0]=best; root[index]=tmp;
        if(score>WINVALUE-100 || score< -WINVALUE+100) break;
    }
    return best;
}
#endif
