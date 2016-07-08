/**
 手写原版  tnyhttpd 源码加注释
**/
/*
  重点是理解在linux多进程当中用管道通信的模型

*/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "ServerL jdbhttpd/0.1.0\r\n"

void accept_request(int);     
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
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
    method[i] = buf[j];
    i++;
    j++;
  }
  method[i] = '\0';
  printf("method: %s\n", method);   //TODO

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

  /*
    client 的请求包是类似这样的
    -----------------------------------------------------------
    GET /wenqian1991/article/details/46011357 HTTP/1.1
    Host: blog.csdn.net
    Connection: keep-alive
    Cache-Control: max-age=0
    Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp;q=0.8
    Upgrade-Insecure-Requests: 1
    User-Agent: Mozilla/5.0 (Windows NT 6.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko)   Chrome/49.0.2623.110 Safari/537.36
    Referer: https://www.google.com/
    Accept-Encoding: gzip, deflate, sdch
    Accept-Language: zh-CN,zh;q=0.8,en;q=0.6
    Cookie: uuid_tt_dd=-3892669430925408839_20160408; _ga=GA1.2.765473712.1460106018;   bdshare_firstime=1460106219320; UN=lqy5589731; UE="lqy82049406@163.com"; BT=1463121186753;  Hm_lvt_6bcd52f51e9b3dce32bec4a3997715ac=1464068810; _JQCMT_ifcookie=1;  _JQCMT_browser=d6577a7eaffa271e19d7ca2e40316b72; lzstat_uv=4179373932551097003|3235372@2955225@3609449;   __utma=17226283.765473712.1460106018.1467184641.1467184641.1;   __utmz=17226283.1467184641.1.1.utmcsr=baidu|utmccn=(organic)|utmcmd=organic;  __message_district_code=440000; uuid=7b98c237-ac5c-4ec9-8789-de5e941d8f74;  aliyungf_tc=AQAAAPVjF3aviAIA6vghebLMX5t7IhqF; __message_sys_msg_id=0; __message_gu_msg_id=0;  __message_cnel_msg_id=0; __message_in_school=0; dc_tos=o9xsjg
    If-None-Match: W/"1620ebe44cee90195723e96fe531ac9f"
-----------------------------------------------------------------
  */
  
  // 然后把 URL 读出来放到 URL 数组中
  // 从下一个不是空字符的 字符开始取值， 那么就可以取得 客户端请求过来的 URL 地址, 从这位开始组合 URL
  while(!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
  {
    url[i] = buf[j];
    i++;
    j++;
  }
  url[i] = '\0';      // 处理完毕就得到了 客户端请求的 url 
  printf("url: %s\n", url);   //TODO
  
  // 如果这个请求是一个 GET 方法的话
  if (strcasecmp(method, "GET") == 0)
  {
    //用一个指针指向 url,  query_string 是一个指针
    query_string = url;     // query_string 这样定义，就是 rul整个字符的指针开始的位置
  
    // 去遍历这个 URL， 跳过字符 ? 前面的所有字符， 如果遍历完毕也没找到字符 ? ， 则退出循环
    while ( (*query_string != '?') && (*query_string != '\0'))
      query_string++;       // 指针的值不断增加 1，  指向 url 的下一个字符
    
    // 退出循环后检查当前的字符是 ? 还是 字符串（url） 的结尾
    if (*query_string == '?') 
    {
      //如果是 ? 的话， 证明这个请求需要调用 cgi, 将 cgi 标志变量为1 （true）
      cgi = 1;
      // 从字符  ? 处把字符串 url 给分隔为两份
      *query_string = '\0';     //NOTE 好好理解一下这里的指针操作
      // 使指针指向字符 ? 后面的那个字符
      query_string++;
    }
  } // END IF GET METHOD  
  printf("query_string: %s\n", query_string);   //TODO
  
  //将前面分隔两份的前面那份字符串， 拼接在字符串 htdocs 的后面之后就输出存储到数组 path 中， 相当于现在 path 中存储着一个字符串
  sprintf(path, "htdocs%s", url);     // 拼接好 web服务器要访问的文件path 
  printf("path: %s\n", path);   //TODO
  
  //如果 path 数组中的这个字符的最后一哥字符是以 / 结尾， 就拼接上一个 "index.html" 字符串， 首页的意思
  if (path[strlen(path) - 1] == '/')
    strcat(path, "index.html");
    
  // 在系统上查询该文件是否存在
  if (stat(path, &st) == -1) 
  {
    // 如果不存在， 那把这次 http 的请求后续的内容( head 和  body ) 全部读完并忽略
    while ( (numchars > 0) && strcmp("\n", buf) )       // strcmp("\n", buf) 似乎是都为负数的？？
      numchars = get_line(client, buf, sizeof(buf));
    
    //然后返回一哥找不到文件的 response 给客户端
    not_found(client);  
  }
  else          // 如果系统上的文件是存在的
  {
    printf("------------- run has file -----------  \n");   //TODO
    // 文件存在， 那去跟常量 S_IFMT 相与， 相与之后的值可以用来判断该文件是什么类型的
    //S_IFMT参读《TLPI》P281，与下面的三个常量一样是包含在<sys/stat.h>
    if ((st.st_mode & S_IFMT) == S_IFDIR){
      printf("------------- visit is DIR -----------  \n");   //TODO
      // 如果这个文件是个目录，那就需要再在 path 后面拼接一个 "/index.html" 的字符串
      strcat(path, "/index.html");
    }
      
      // S_IXUSR, S_IXGRP, S_IXOTH三者可以参考  <TLPI> P295
      // 如果这个文件是一个可执行的文件， 不论是属于用户/组/其他这三者任何类型的, 就将 cgi 标志变量设为1 
    if ( (st.st_mode & S_IXUSR) ||
         (st.st_mode & S_IXGRP) ||
         (st.st_mode & S_IXOTH)      )
    {
      cgi = 1;  
    }
  
    // TODO 这里来判断是否是执行 CGI 还是 执行 普通的文件
    if (!cgi) {
      printf("------------- serve_file static file -----------  \n");   //TODO
      serve_file(client, path);       // 如果不需要cgi机制的话
    }
    else {
      execute_cgi(client, path, method, query_string);    //如果需要则调用cgi 
    }
      
  }
  close(client);      // 关闭客户端的socket连接
}


/**
 通知客户端连接有问题，返回有问题的连接提示, 发送 400 HTTP 给客户端
**/
void bad_request(int client)
{
  char buf[1024];
  
  sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");   //写入buf
  send(client, buf, sizeof(buf), 0);
  
  sprintf(buf, "Content-type: text/html\r\n");      //重新写入buf
  send(client, buf, sizeof(buf), 0);
  
  sprintf(buf, "\r\n");
  send(client, buf, sizeof(buf), 0);        //发送断行数据给客户端
  
  sprintf(buf, "<p>Your browser sent a bad request, ");
  send(client, buf, sizeof(buf), 0);
  
  sprintf(buf, "such as a POST without a Content-Length.\r\n");
  send(client, buf, sizeof(buf), 0);
  
}


/**
  返回整个文件内容给 客户端socket， 类似linux 的系统命令 cat
  /* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 * FILE pointer for the file to cat 
**/
void cat(int client, FILE *resource)
{
  char buf[1024];
  
  //从文件描述符中读取指定内容
  fgets(buf, sizeof(buf), resource);      // 假如文件 大于  1024， 那这里不是内存泄漏？？？
  while (!feof(resource))
  {
    send(client, buf, strlen(buf), 0);    // 不会内存泄漏， 是先读取1024， 下面的逻辑循环读取1024， 然后持续发送给 client
    fgets(buf, sizeof(buf), resource);
  }
}


/*
 返回一个 无法执行的 cgi 信息给客户端 
*/
void cannot_execute(int client)
{
  char buf[1024];
  
  sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "<p>Error prohibited CGI execution.\r\n");
  send(client, buf, strlen(buf), 0);
}

/*
  打印错误信息
  Print out an error message with perror() (for system errors; based
  on value of errno, which indicates system call errors) and exit the
  program indicating an error
*/
void error_die(const char *sc)
{
  //包含于<stdio.h>,基于当前的 errno 值，在标准错误上产生一条错误消息。参考《TLPI》P49
  perror(sc);
  exit(1);
}

/*
 执行 cgi 程序脚本， 需要设置相应的环境变量， 比如如果你要跑 PHP 程序， 那么linux 的环境 env 必须要可以执行PHP程序 
 
 ------------------------------------------------------
 这里的理解重点就是 为什么要用两个 管道来做？？？
 因为 linux 系统的 cgi 都是标准输出输入的 ，  STDIN,  STDOUT,  如果要把标准的输出输入获得，那么有一种方式
 就是把 标准的输出输入 导向 到管道去
 那为什么用两个管道 而不是 一个呢？？
 因为本来一条管理就是 可以理解为 就是储存一个信息的， 只是 一端是写信息， 一端是读信息
 既然我们需要 一个标准输入，  一个标准输出， 那么我们就需要两个管道
 
 关于命名的问题
 cgi_input 代表  STDIN
管道的操作情况，  子进程的时候， 把标准输入  STDIN的句柄 赋给 管道 cgi_input 的读端  cgi_input[0], STDIN 是读取的，也就是读取 管道的内容, 然后在父进程接收大 客户端请求的时候， 就
 
 cgi_output 代表  STDOUT
管道的操作情况，  子进程的时候， 把标准输出  STDOUT的句柄 赋给 管道 cgi_output 的写端  cgi_input[1], STDOUT是输出的，也就是输出到 管理的写端
 ------------------------------------------------------
*/
void execute_cgi(int client, const char *path, 
                  const char *method, const char *query_string)
{
  char buf[1024];
  int cgi_output[2];        // 管道？？？
  int cgi_input[2];         // 管道？？？
  
  pid_t pid;                // 声明线程
  int status;
  int i;
  char c;                   // 声明单个字符？？？
  int numchars = 1;
  int content_length = -1;
  
  // 往  buf 中填东西以保证能进入下面的 while
  buf[0] = 'A';
  buf[1] = '\0';
  // 如果 http 请求是 GET 方法的话，读取并忽略请求剩下的内容
  /*
     看这个条件
     while (strcmp("\n", buf))
     原来一直以为 是要  strcmp("\n", buf) 大于 0 ， 整个逻辑才会执行， 现在才知道不是那么回事， 在 C里面只要不为0 ， 条件都是 真的， 也就是说  strcmp("\n", buf) 即使是为负数， 该条件也成立，也可以执行
  */
  if (strcasecmp(method, "GET") == 0)
  {
    while ((numchars > 0) && strcmp("\n", buf))
      numchars = get_line(client, buf, sizeof(buf));
  }
  else        // 如果是 POST 方法
  {
    //只有 POST 方法的时候才继续读内容
    numchars = get_line(client, buf, sizeof(buf));
    //这个循环的目的是读出指示  body  长度大小的参数， 并记录 body 的长度大小， 其余的 header 里面的参数一律忽略
    // 注意这里只读完 header 的内容， body 的内容没有读
    while ((numchars > 0) && strcmp("\n", buf)) 
    {
      /*
        分析下 HTTP 头， 类似下面这样的， 这个就是上面的 header
        ---------------------------------------------------
        
        HTTP/1.1 200 OK          
        Content-Type: text/html;charset=gb2312
        Server: Microsoft-IIS/7.5
        X-Powered-By: PHP/5.3.29
        X-Powered-By: ASP.NET
        Date: Wed, 06 Jul 2016 06:43:24 GMT
        Content-Length: 13889     ----->  "Content-Length:"这一行刚好占了 15  个字符，所以下面从 buf[15] 开始算起
        
        ------------------------------------------------------------
      */
      buf[15] = '\0';
      if (strcasecmp(buf, "Content-Length:") == 0)
        content_length = atoi(&(buf[16]));    //记录 body 的长度大小
        /*
            上面的逻辑是， 当读取到行的内容是这样的  "Content-Length: 13889", 前15个字符 "Content-Length:",那么上面的写法   atoi(&(buf[16]))  就是从  buf 的第 16 位 开始， 把 buf 剩下的内容转为 整型数据
        */
      numchars = get_line(client, buf, sizeof(buf));
    }
    
    // 如果 HTTP 请求的 header 部分没有指示  body 的长度大小的数值， 则报错返回
    if (content_length == -1)
    {
      bad_request(client);
      return;
    }
  }     // END IF POST METHOD

  // 上面的逻辑， 如果是 GET， 只是获取了  numchars的数值， 如果是 POST，  除了获取了  numchars 之外， 还获取了  content_length 的值
  
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  
  // 下面这里创建两个管道, 用于两个进程间通信
  if (pipe(cgi_output) < 0) {
    cannot_execute(client);
    return;
  }
  
  if (pipe(cgi_input) < 0) {
    cannot_execute(client);
    return;
  }
  
  //创建一个子进程
  if ( (pid = fork()) < 0 ) {
    cannot_execute(client);
    return;
  }
  
  // 子进程用来执行  cgi 脚本
  if (pid == 0)         // pid 为 0， 子进程创建成功, 在子进程
  {
    char meth_env[255];
    char query_env[255];
    char length_env[255];
    
    /*
      TODO 这里是理解的重点
      0 是读端，   1 是写端
    */
    //将 子进程的输出 由标准输出重定向到  cgi_output 的管道写端上
    dup2(cgi_output[1], 1);  // 应该等于  dup2(cgi_output[1], STDOUT);
    
    //将 子进程的输出 由标准输入重定向到  cgi_output 的管道读端上
    dup2(cgi_input[0], 0);  // 应该等于  dup2(cgi_output[0], STDIN);
    
    // 标准 API 为  STDIN = 0,  STDOUT = 1  
    
    // 关闭 cgi_output 管道的读端与  cgi_input 端到的写端
    close(cgi_output[0]);
    close(cgi_input[1]);
    
    // 构造一个环境变量
    sprintf(meth_env, "REQUEST_METHOD=%s", method);
    //将这个环境变量加进子进程的运行环境中
    putenv(meth_env);
    
    // 根据 http 请求的不同方法， 构造并存储不同的环境变量
    if (strcasecmp(method, "GET") == 0) {
      sprintf(query_env, "QUERY_STRING=%s", query_string);
      putenv(query_env);
    }
    else      // POST
    {
      sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
      putenv(length_env);
    }
    
    // TODO 当进入子进程的时候， 只是设置了一些环境变量 ??
    // 最后讲子进程替换成另一个进程并执行 cgi 脚本
    execl(path, path, NULL);
    exit(0);
  }
  else        // 在父进程
  {
    // 父进程则关闭了  cgi_output 管理的写端 和  cgi_input 管道的读端
    close(cgi_output[1]);
    close(cgi_input[0]);
    
    // 如果是 POST 方法的话就继续读取 body 的内容， 并写到 cgi_input 管道里让子进程去读取
    if (strcasecmp(method, "POST") == 0) 
    {
      for (i = 0; i < content_length; i++) 
      {
        recv(client, &c, 1, 0);         // 定义了 char c 之后， 第一次在这里使用， 一个一个字符接收
        write(cgi_input[1], &c, 1);     //一个一个字符写入
      }
    }
    
    // 然后从  cgi_output 管道中读取子进程的输出， 并发送到客户端去
    while(read(cgi_output[0], &c, 1) > 0) {
      send(client, &c, 1, 0);
    }
    
    //关闭管道
    close(cgi_output[0]);
    close(cgi_input[1]);
    //等待子进程的退出
    waitpid(pid, &status, 0);
  }
}


/**
* 获得读取到 socket 的内容（字符）, 这个函数最关键的就是 逐行逐行读取的， 而不是一次读取的，是逐行读取 HTTP 包的信息
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
    //printf("%02X\n", c);
    
    if (n > 0)      // 如果能读到内容, n > 0, 表示如果能一直读取到内容的话
    {
      if (c == '\r')      // 如果读取到换行符, HTTP包第一次换行符
      {
          n = recv(sock, &c, 1, MSG_PEEK);
          //printf("%2X\n", c);
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


// 返回 HTTP 的 header信息
void headers(int client, const char *filename)
{
  char buf[1024];
  (void)filename;
  
  strcpy(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  
  strcpy(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);

  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  
  strcpy(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
}


/*
 返回给客户端 404 not found 状态消息
*/
void not_found(int client) 
{
  char buf[1024];
  
  sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
  send(client, buf, strlen(buf), 0);

  sprintf(buf, "your request because the resource specified\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "is unavailable or nonexistent.\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}


/*
  发送一个符合的文件信息给客户端， 如果发生故障，会告诉客户端
*/
void serve_file(int client, const char *filename)
{
  printf(" ------------  static file ------------\n");
  FILE *resource = NULL;
  int numchars = 1;
  char buf[1024];
  
  //确保 buf 里面有东西， 能进入下面的 while 循环
  buf[0] = 'A';
  buf[1] = '\0';
  
  // 循环作用是读取并忽略掉这个 http 请求后面的所有内容
  printf(" start while: \n");
  while ((numchars > 0) && strcmp("\n", buf)) {
    numchars = get_line(client, buf, sizeof(buf));
    printf(" ------ dead in while ------ \n");
  }
  printf(" numchars: %d\n", numchars);
  
  // 打开这个传进来的这个路径所指的文件
  resource = fopen(filename, "r");
  if (resource == NULL) {
    printf(" file not found \n");
    not_found(client);
  }
  else 
  {
    printf(" file is found \n");
    // 打开成功后， 将这个文件的基本信息封装成  response 的头部(header) 并返回
    headers(client, filename);
    //接着把这个文件的内容读出来作为 response  的 body 发送到客户端
    cat(client, resource);
  }
  
  fclose(resource);
}


/*
  处理连接到 服务器 指定端口的客户端连接
  
*  This function starts the process of listening for web connections
* on a specified port.  If the port is 0, then dynamically allocate a
* port and modify the original port variable to reflect the actual
* port.

* 返回 socket 连接句柄
*/
int startup(u_short *port) 
{
  int httpd = 0;
  //sockaddr_in 是 IPV4 的套接字地址结构
  struct sockaddr_in name;
  
  //socket() 用于创建一个用于 socket 的描述符
  //这里的 PF_INET 其实是与 AF_INET 同义
  httpd = socket(PF_INET, SOCK_STREAM, 0);
  if (httpd == -1)
    error_die("socket");
    
  memset(&name, 0, sizeof(name));   // 给 name 设置一段内存
  name.sin_family = AF_INET;
  
  //将 *port 转换成以网络字节序表示的 16 位整数
  name.sin_port = htons(*port);
  //大多实现都将其 定义成了  0.0.0.0 
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  
  //如果传进去的 sockaddr 结构中的 sin_port 指定为 0,  这时系统会选择一个临时的端口号
  if ( bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0 )
    error_die("bind");
    
  // 如果调用 bind 后端口号仍然是 0， 则手动调用 getsockname() 获取端口号
  if (*port == 0)
  {
    int namelen = sizeof(name);
    // 调用 getsockname() 获取系统给 httpd 这个 socket 随机分配的端口号
    if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
      error_die("getsockname");
    *port = ntohs(name.sin_port);
  } 
  
  //最初的 BSD socket 实现中， backlog 的上限是5
  if (listen(httpd, 5) < 0)
    error_die("listen");
    
  return(httpd);
}


/*
  反馈请求的方法无法执行的信息给客户端
*/
void unimplemented(int client)
{
  char buf[1024];
  
  sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "</TITLE></HEAD>\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
  send(client, buf, strlen(buf), 0);
  
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

int main(void)
{
  int server_sock = -1;
  u_short port = 0;
  int client_sock = -1;
  
  //sockaddr_in 是 IPV4 的套接字地址结构
  struct sockaddr_in client_name;
  int client_name_len = sizeof(client_name);
  
  server_sock = startup(&port);
  printf("httpd running on port %d\n", port);
  
  while(1)
  {
    //阻塞等待客户端的连接
    client_sock = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);
    
    if (client_sock == -1)
      error_die("accept");
    
    accept_request(client_sock);
  }
  
  close(server_sock);
  
  return 0;
}








  
