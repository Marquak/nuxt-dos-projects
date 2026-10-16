/* Rules tests compile unchanged on the host and on 16-bit DOS. */
static void empty_board(Board *b) {memset(b,0,sizeof(*b));b->side=1;b->forced=-1;}
static void put(Board *b,int r,int c,int p) {b->cell[square(r,c)]=(S8)p;}
static unsigned long perft(Board b,int depth) {
    Move m[MAXMOVES];int i,n,ended;unsigned long total=0;Board c;
    if(!depth)return 1;
    n=moves(&b,m);
    for(i=0;i<n;++i) {c=b;ended=apply(&c,m[i]);total+=perft(c,depth-ended);}
    return total;
}
static void coretests(void) {
    Board b,c;Move m[MAXMOVES],best;int n,i,j,count;
    engine_init();board_start(&b);history_reset(&b);
    check(moves(&b,m)==7,"seven opening moves");
    check(perft(b,2)==49,"opening perft depth 2");
    check(perft(b,3)==302,"opening perft depth 3");
    check(perft(b,4)==1469,"opening perft depth 4");
    for(i=0;i<32;++i)check(square(rowof(i),colof(i))==i,"square-coordinate round trip");
    empty_board(&b);put(&b,5,0,1);put(&b,4,1,-1);put(&b,2,3,-1);put(&b,0,7,-1);put(&b,5,6,1);
    n=moves(&b,m);check(n==1&&m[0].over==square(4,1),"capture excludes quiet alternatives");
    check(!apply(&b,m[0])&&b.side==1&&b.forced==square(3,2),"jump continuation retains side");
    n=moves(&b,m);check(n==1&&m[0].from==b.forced&&m[0].to==square(1,4),"only the jumping man continues");
    check(apply(&b,m[0])&&b.side==-1&&b.forced==-1,"complete chain ends turn");
    check(b.cell[square(4,1)]==0&&b.cell[square(2,3)]==0,"both captures removed");
    empty_board(&b);put(&b,2,1,1);put(&b,1,2,-1);put(&b,1,4,-1);
    n=moves(&b,m);check(n==1,"promotion capture generated");
    check(apply(&b,m[0])&&b.cell[square(0,3)]==2&&b.side==-1,"promotion ends capturing turn");
    check(b.cell[square(1,4)]==-1,"new king cannot immediately jump backwards");
    empty_board(&b);put(&b,4,3,1);put(&b,5,4,-1);
    check(!can_jump(&b,square(4,3)),"men cannot capture backwards");
    b.cell[square(4,3)]=2;check(can_jump(&b,square(4,3)),"kings capture backwards");
    empty_board(&b);put(&b,4,3,2);put(&b,3,2,-1);put(&b,1,2,-1);put(&b,3,4,-1);
    n=moves(&b,m);check(n==2,"both capture routes legal regardless of length");
    c=b;apply(&c,m[0]);check(c.forced>=0,"longer route must finish");
    c=b;check(apply(&c,m[1]),"shorter chosen route may end normally");
    empty_board(&b);put(&b,0,1,1);check(moves(&b,m)==0,"blocked side has no legal moves");
    empty_board(&b);check(moves(&b,m)==0,"empty side has no legal moves");
    empty_board(&b);put(&b,5,0,2);put(&b,2,7,-2);b.quiet=79;
    n=moves(&b,m);apply(&b,m[0]);check(b.quiet==80,"80 quiet king turns reach draw limit");
    empty_board(&b);put(&b,5,0,1);put(&b,2,7,-2);b.quiet=79;
    moves(&b,m);apply(&b,m[0]);check(b.quiet==0,"man move resets no-progress counter");
    history_reset(&b);history_add(&b);check(!repeated(&b),"irreversible positions reset history");
    b.quiet=1;history_reset(&b);history_add(&b);check(!repeated(&b),"two positions not threefold");
    history_add(&b);check(repeated(&b),"three identical positions draw");
    c=b;c.side=-c.side;check(!repeated(&c),"repetition includes side to move");
    empty_board(&b);put(&b,5,0,1);put(&b,4,1,-1);history_reset(&b);
    best=computer(&b,2,5000UL,0);check(best.over==square(4,1),"AI takes forced winning capture");
    c=b;apply(&c,best);check(moves(&c,m)==0,"AI winning capture leaves no opponent");
    board_start(&b);history_reset(&b);c=b;
    best=computer(&b,1,3000UL,0);n=moves(&b,m);count=0;
    for(i=0;i<n;++i)if(!memcmp(m+i,&best,sizeof(Move)))++count;
    check(count==1,"bounded AI returns legal move");
    check(!memcmp(&b,&c,sizeof(Board)),"search leaves original position untouched");
    check(nodes<=3000&&completed_depth>=1,"node budget with completed iteration");
    best=computer(&b,2,1,0);n=moves(&b,m);count=0;
    for(i=0;i<n;++i)if(!memcmp(m+i,&best,sizeof(Move)))++count;
    check(count==1,"tiny search budget still returns legal fallback");
    /* Multiple complete deterministic games exercise make/generate invariants. */
    for(j=0;j<4;++j) {
        board_start(&b);history_reset(&b);
        for(i=0;i<160;++i) {
            int red=0,blue=0,s;
            n=moves(&b,m);if(!n||b.quiet>=80||repeated(&b))break;
            count=(i*7+j*13)%n;c=b;apply(&b,m[count]);
            for(s=0;s<32;++s){if(b.cell[s]>0)++red;if(b.cell[s]<0)++blue;}
            check(red<=12&&blue<=12,"piece counts never increase");
            check(b.forced<0||can_jump(&b,b.forced),"forced square has a continuation");
            if(b.side!=c.side)history_add(&b);
        }
    }
}
