/**
 手写原版  tnyhttpd 源码加注释
**/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "ServerL jdbhttpd/0.1.0\r\n"

void accept_request(int);     
void bad_request(int);
void cat(int FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void server_file(int, const char *);
int startup(u_short *);         // u_short 类型用途？？减少内存吗？？
void unimplemented(int);


//接口请求的函数
void accept_request(int client) 
{
  /**
    我们发现很多的长度都设置为  255, 512, 1024 这样有利用内存的利用， 提高效率
  **/
  char buf[1024];
  int numchars;
  char method[255];
  char url[255];
  char path[512];
  size_t i, j;
  struct stat st;
  int cgi = 0;        //　当程序运行 cgi 的时候， 这个值为 true
  
  char *query_string = NULL;
  
  numchars = get_line(client, buf, sizeof(buf));      //返回一共读了多少个字符, 改写了 buf 变量
  /**
    下面的源码有两个主线的变量 i, j ， 如何理解这两个主线变量的变化呢？？？ 我们可以归纳这种处理方式为一种常见的手法， 通常我们有一个 很大的数据包的时候， 数据包里面可能有几类内容 ， 有A内容， B内容 等
    
    那么 i  就是可以理解为， 当我们开始取 A内容，  或者开始取 B内容的时候， i 的知道就会初始化为 0 ， 以去匹配我们A， B的类容，满足程序的逻辑处理
    
    
    j  可以理解为， 从我们一开始接收这个 数据包的时候， j 就开始一直追踪数据包的容量（也就是字符的数量）， 只有在我们要取其中的 A, B 内容的时候， 用例如下面的逻辑：
    
    A[i]   =    data[j];
    B[i]   =    data[j]
    
    来分别填充 A， B 的内容,  下面的逻辑也是这样
      
  **/
  i = 0; 
  j = 0;
  while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
  {
    // 读取到 buf[j] isspace 的时候， 证明 HTTP 已经换行了， 所以第一次换行， 得到的就是 HTTP 头的内容，里面包含了 HTTP 的方法
    method[i] = buf[j]
    i++;
    j++;
  }
  method[i] = '\0';

  //如果请求的方法不是 GET 或者 POST 任意一种的话，就直接发送 response 告诉客户端没实现该方法
  if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
  {
    unimplemented(client);
    return;
  }
  
  // 如果是 POST 方法 就将 cgi 标志变量改为 1 （true）
  if ( strcasecmp(method, "POST") == 0 )
    cgi = 1;

  
  i = 0;
  //跳过所有的空白字符（空格）
  while (ISspace(buf[j]) && j < sizeof(buf))
    j++;
  
  // 然后把 URL 读出来放到 URL 数组中
  while(!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
  {
    url[i] = buf[j];
    i++;
    j++;
  }
  url[i] = '\0';
  
  // 如果这个请求是一个 GET 方法的话
  if (strcasecmp(method, "GET") == 0)
  {
    
  }
}



/**
* 获得读取到 socket 的内容（字符）
* 
*@return: 总的字符数
**/
int get_line(int sock, char *buf, int size) 
{
  int i = 0;
  char c = '\0';
  int n;
  
  while ( (i < size - 1) && (c != '\n'))    //????
  {
    n = recv(sock, &c, 1, 0);
    printf("%02X\n", c);
    
    if (n > 0)      // 如果能读到内容, n > 0, 表示如果能一直读取到内容的话
    {
      if (c == '\n')      // 如果读取到换行符, HTTP包第一次换行符
      {
          n = recv(sock, &c, 1, MSG_PEEK);
          printf("%2X\n", c);
          if ((n > 0) && (c == '\n'))     // 如果读取到 HTTP 包第二次换行符
            recv(sock, &c, 1, 0);
          else
            c = '\n';
      } // END IF c == '\n'
      
      buf[i] = c;           // 把读取到的 c 的值 一个一个组合到 buf数组
      i++;
    } //END IF n > 0
    else 
    {
      c = '\n';
    }
  }   // END WHILE
  
  buf[i] = '\0';
  return(i);  
}




