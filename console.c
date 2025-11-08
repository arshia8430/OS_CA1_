// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.
#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

#include "fcntl.h"
#include "fs.h"   
#include "file.h"

////////////////////////////////////
#include "kbd.h"
#define C(x)  ((x)-'@') 



// --- Variables for Full State Snapshot & Restore ---

///////////////////////////////////

#define HISTORY_SIZE 128
struct {
  uint pos; 
} insert_history[HISTORY_SIZE];
int history_pos;

static uint prev_line_len = 0;
static void consputc(int);

static int panicked = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
  int pos;
  outb(CRTPORT, 14); pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15); pos |= inb(CRTPORT+1);
  if(c == '\n') pos += 80 - pos%80;
  else if(c == BACKSPACE){ if(pos > 0) --pos; }
  else {
    if((c & 0xff00) == 0)
      crt[pos++] = (c & 0xff) | 0x0700; 
    else
      crt[pos++] = c & 0xffff; 
  }
  if(pos < 0 || pos > 25*80) panic("pos under/overflow");
  if((pos/80) >= 24){
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }
  outb(CRTPORT, 14); outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15); outb(CRTPORT+1, pos);
  crt[pos] = ' ' | 0x0700;
}
void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
  uint cursor;
  int start_pos;
} input;


#define MAX_MATCHES 32
#define MAX_FILENAME_LEN 16

int tab_press_count;                        
char last_prefix[INPUT_BUF];
int autocomplete_active = 0;
char saved_buf[INPUT_BUF];
uint saved_r, saved_w, saved_e, saved_cursor;
int select_mode;     
uint select_anchor;  
uint select_start;   
uint select_end;      

char clipboard[INPUT_BUF];
int clipboard_len;
static int get_cursor_pos(void)
{
  int pos;
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);
  return pos;
}
////////////////////////////////////////////////////////
static void set_cursor_pos(int pos)
{
  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
}


void redraw_line(void)
{
  uint current_line_len = input.e - input.w;
  set_cursor_pos(input.start_pos);
  for(int i = input.w; i < input.e; i++){
    char c = input.buf[i % INPUT_BUF];
    if(select_mode == 2 && i >= select_start && i < select_end) {
      consputc(c | 0x7000); 
    } else {
      consputc(c);
    }
  }

  if(current_line_len < prev_line_len){
    for(int i = 0; i < prev_line_len - current_line_len; i++){
      consputc(' ');
    }
  }
  prev_line_len = current_line_len;
  int final_pos = input.start_pos + (input.cursor - input.w);
  set_cursor_pos(final_pos);
}
/////////////////////////////////////////////////////////////

static void history_adjust_after_insert(uint ins_pos) {
  for(int i = 0; i < history_pos; i++) {
    if(insert_history[i].pos >= ins_pos) {
      insert_history[i].pos++;
    }
  }
}

static void history_adjust_after_delete(uint del_pos) {
  for(int i = 0; i < history_pos; i++) {
    if(insert_history[i].pos > del_pos) {
      insert_history[i].pos--;
    }
  }
}

/////////////////////////////////////////////////////////////

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;
  acquire(&cons.lock);
  while((c = getc()) >= 0){
    if(c == 0)
      continue;
    if (select_mode == 2) {
      if (c == KEY_LF || c == KEY_RT || c == C('A') || c == C('D') ||
          c == C('S') || c == C('Z') || c == C('V')) {
        select_mode = 0;
        redraw_line(); 
        continue;      
      }
      if (c == C('C')) {
        clipboard_len = select_end - select_start;
        if(clipboard_len > INPUT_BUF) clipboard_len = INPUT_BUF;
        memmove(clipboard, &input.buf[select_start], clipboard_len);
        continue; 
      }

      {
      uint start = select_start;
      uint end = select_end;
      uint len = end - start;
      select_mode = 0; 
      int replacement_len = (c == C('H') || c == '\x7f') ? 0 : 1;
      if (len != replacement_len) {
          memmove(&input.buf[start + replacement_len], &input.buf[end], input.e - end);
      }

      input.e = input.e - len + replacement_len;
      if (replacement_len > 0) {
          input.buf[start] = c;
          history_adjust_after_insert(start);
          if(history_pos < HISTORY_SIZE) { insert_history[history_pos++].pos = start; }
      } else {
          for(int i = 0; i < len; i++) {
              history_adjust_after_delete(start);
          }
      }

      input.cursor = start + replacement_len;
      redraw_line();
    }
    continue; 
  }
    switch(c){
    case C('P'):  
      tab_press_count = 0;
      last_prefix[0] = '\0';
      doprocdump = 1;
      break;
    case C('U'):
      tab_press_count = 0;
      last_prefix[0] = '\0';
      while(input.e != input.w){ input.e--; consputc(BACKSPACE); }
      input.cursor = input.w;
      break;
    case C('H'): case '\x7f':
      tab_press_count = 0;
      last_prefix[0] = '\0';
      if(input.cursor > input.w){
        history_adjust_after_delete(input.cursor - 1);
        memmove(&input.buf[input.cursor - 1], &input.buf[input.cursor], input.e - input.cursor);
        input.e--;
        input.cursor--;
        redraw_line();
      }
      break;
    case C('Z'):
      tab_press_count = 0;
      last_prefix[0] = '\0';
      if(history_pos > 0){
        history_pos--;
        uint pos_to_delete = insert_history[history_pos].pos;
        if(pos_to_delete < input.e) {
          memmove(&input.buf[pos_to_delete], &input.buf[pos_to_delete + 1], input.e - (pos_to_delete + 1));
          input.e--;
          history_adjust_after_delete(pos_to_delete);
          input.cursor = pos_to_delete;
          redraw_line();
        }
      }
      break;
    case C('S'):
      tab_press_count = 0;
      last_prefix[0] = '\0';
      if (select_mode == 0) {
        select_anchor = input.cursor;
        select_mode = 1;
      } else { 
        select_mode = 2;
        if (input.cursor < select_anchor) {
          select_start = input.cursor;
          select_end = select_anchor + 1;
        } else {
          select_start = select_anchor;
          select_end = input.cursor + 1;
        }
        if(select_start >= select_end - 1) 
            select_mode = 0;
        redraw_line();
      }
      break;
    case C('V'):
      tab_press_count = 0;
      last_prefix[0] = '\0';
      if (clipboard_len > 0) {
        if(input.e + clipboard_len < INPUT_BUF) {
          memmove(&input.buf[input.cursor + clipboard_len], &input.buf[input.cursor], input.e - input.cursor);
          memmove(&input.buf[input.cursor], clipboard, clipboard_len);
          input.e += clipboard_len;
          input.cursor += clipboard_len;
          redraw_line();
        }
      }
      break;
    case KEY_LF:
      tab_press_count = 0;
      last_prefix[0] = '\0';
      if(input.cursor > input.w){
        input.cursor--;
        set_cursor_pos(get_cursor_pos() - 1);
      }
      break;
    case KEY_RT:
        tab_press_count = 0;
        last_prefix[0] = '\0';
      if(input.cursor < input.e){
        input.cursor++;
        set_cursor_pos(get_cursor_pos() + 1);
      }
      break;
    case C('D'):
      {
        tab_press_count = 0;
        last_prefix[0] = '\0';
        uint next_word_pos = input.cursor;
        while(next_word_pos < input.e && input.buf[next_word_pos % INPUT_BUF] == ' ') next_word_pos++;
        while(next_word_pos < input.e && input.buf[next_word_pos % INPUT_BUF] != ' ') next_word_pos++;
        while(next_word_pos < input.e && input.buf[next_word_pos % INPUT_BUF] == ' ') next_word_pos++;
        if (next_word_pos < input.e) {
          input.cursor = next_word_pos;
          set_cursor_pos(input.start_pos + (input.cursor - input.w));
        }
      }
      break;
    case C('A'):
      {
        tab_press_count = 0;
        last_prefix[0] = '\0';
        uint i = input.cursor;
        int at_boundary = (i == input.w || input.buf[(i - 1) % INPUT_BUF] == ' ');
        if(at_boundary && i > input.w) {
          i--;
          while(i > input.w && input.buf[(i - 1) % INPUT_BUF] == ' ') i--;
        }
        while(i > input.w && input.buf[(i - 1) % INPUT_BUF] != ' ') i--;
        input.cursor = i;
        set_cursor_pos(input.start_pos + (input.cursor - input.w));
      }
      break;

    case '\n':
      input.buf[input.e++ % INPUT_BUF] = '\n';
      consputc('\n');
      input.w = input.e;
      wakeup(&input.r);
      input.cursor = input.w;
      input.start_pos = 0;
      select_mode = 0;
      history_pos = 0;
      break;

case '\t':
  if (autocomplete_active == 0) {
    autocomplete_active = 1;
    tab_press_count++;
    memmove(saved_buf, input.buf, sizeof(input.buf));
    saved_r = input.r;
    saved_w = input.w;
    saved_e = input.e;
    saved_cursor = input.cursor;
    input.buf[input.e++ % INPUT_BUF] = '\t';
    input.w = input.e;
    wakeup(&input.r);
  }
  break;
    default:
      if(c != '\n' && c != 0 && input.e - input.r < INPUT_BUF){
        if(c == '\r') break;
        if(input.e == input.w) {
          input.start_pos = get_cursor_pos();
        }
        history_adjust_after_insert(input.cursor);
        if(history_pos < HISTORY_SIZE){
          insert_history[history_pos++].pos = input.cursor;
        }
        if(input.cursor < input.e) {
          memmove(&input.buf[input.cursor + 1], &input.buf[input.cursor], input.e - input.cursor);
        }
        input.buf[input.cursor % INPUT_BUF] = c;
        input.e++;
        input.cursor++;
        redraw_line();
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();
  }
}
int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

////////////////////////////
history_pos = 0;
  input.cursor = input.w;
    clipboard_len = 0;
  select_mode = 0;
  prev_line_len=0;
tab_press_count = 0;
last_prefix[0] = '\0';
///////////////////////////
  ioapicenable(IRQ_KBD, 0);
}

void
console_print_and_redraw(char *matches_str)
{
  acquire(&cons.lock);
  memmove(input.buf, saved_buf, sizeof(input.buf));
  input.r = saved_r;
  input.w = saved_w;
  input.e = saved_e;
  input.cursor = saved_cursor;

  char *current_match = matches_str;
  char *next_match = 0;

  for (char *p = current_match; *p != '\0'; p++) {
      if (*p == '\n') {
          next_match = p;
          break;
      }
  }
  
  int is_multiple = (next_match != 0 && *(next_match + 1) != '\0');
  if (!is_multiple) { 
    int prefix_start = input.cursor;
    while(prefix_start > input.w && input.buf[(prefix_start-1) % INPUT_BUF] != ' '){
        prefix_start--;
    }
    int prefix_len = input.cursor - prefix_start;
    int completion_len = strlen(current_match);
    if(completion_len > 0 && current_match[completion_len-1] == '\n'){
        completion_len--; 
    }

    for (int i = prefix_len; i < completion_len; i++) {
        char c = current_match[i];
        
        history_adjust_after_insert(input.cursor);
        if(history_pos < HISTORY_SIZE){ insert_history[history_pos++].pos = input.cursor; }

        if(input.cursor < input.e) {
          memmove(&input.buf[input.cursor + 1], &input.buf[input.cursor], input.e - input.cursor);
        }
        input.buf[input.cursor % INPUT_BUF] = c;
        input.e++;
        input.cursor++;
    }
    
  } else if(tab_press_count > 1) { 
        int original_locking = cons.locking;
        cons.locking = 0; 

        consputc('\n');
        for(char *p = matches_str; *p != '\0'; p++) {
            consputc(*p == '\n' ? ' ' : *p);
        }
        consputc('\n');
        cprintf("$ ");
        cons.locking = original_locking;
        input.start_pos = get_cursor_pos();
        for(int i = input.w; i < input.e; i++) {
          consputc(input.buf[i % INPUT_BUF]);
        }
        int final_pos = input.start_pos + (input.cursor - input.w);
        set_cursor_pos(final_pos);
  }
  autocomplete_active = 0;
  redraw_line();  
  release(&cons.lock);
}