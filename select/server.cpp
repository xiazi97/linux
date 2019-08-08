//基于select模型的TCP服务器
/*
 select函数原型：
#inclued <sys/select.h>
int select(int nfds,fd_set *readfds,fd_set *writefds,
	  fd_set  *exceptfds,struct timeval *timeout);

nfds 是需要监视的最大文件描述符值 + 1
fd_set底层实际是一个位图，既是输入型参数又是输出型参数，所以每次要清零重新设定 
timeout 用来设置select()的等待时间

fd_set接口：
void FD_CLR(int fd,fd_set *set);  //用来清楚描述词组set中相关fd的位
int FD_ISSET(int fd,fd_set *set); //用来测试描述词组set中相关fd的位是否为真
void FD_SET(int fd,fd_set *set);  //用来设置描述词组set中相关fd的位
void FD_ZERO(fd_set *set);        //用来清除描述词组set的全部位

函数返回值：
	执行成功返回文件描述符词组已改变的个数
* /

/*
1.将服务器所监听的socket添加至FD中
2.select这个FD中对应的readset集，看是否有读就绪，若有则是新的连接到来
3.accept函数返回这个新连接的socket，同时也添加到FD中
4.循环检测fd中各个socket中的状态，若监听socket有读就绪，说明有新连接到来
  若是其他的socket有读就绪，说明当前连接有数据到来
 */

#include <iostream>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
using namespace std;

void Init(int* fd_list,int fd_list_size)
{
    for(int i=0;i<fd_list_size;++i){
        fd_list[i] = -1;
    }
}


void Reload(int listen_fd,int* connect_list,int connect_list_size,
            fd_set* read_fds,int* max_fd)
{
    FD_ZERO(read_fds);
    FD_SET(listen_fd,read_fds);
    int max = listen_fd;
    for(int i=0;i<connect_list_size;++i){
        if(connect_list[i] != -1){
            FD_SET(connect_list[i],read_fds);
            if(connect_list[i] > max){
                max = connect_list[i];
            }
        }
    }
    *max_fd = max;
}

void Add(int fd,int* connect_list,int connect_list_size)
{
    for(int i=0;i<connect_list_size;++i){
        if(connect_list[i] == -1){
            connect_list[i] = fd;
            break;
        }
    }
    return;
}
void Usage()
{
    cout << "usage: ./server ip port\n" <<endl;
}
int main(int argc,char* argv[])
{
    if(argc != 3){
        Usage();
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    addr.sin_port = htons(atoi(argv[2]));

    int listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd < 0){
        perror("create socket error");
        return 1;
    }

    int ret = bind(listen_fd,(struct sockaddr*)&addr,sizeof(addr));
    if(ret < 0){
        perror("bind error");
        return 1;
    }

    ret = listen(listen_fd,5);
    if(ret < 0){
        perror("listen error");
        return 1;
    }

    fd_set read_fds;
    int fd_list[1024];
    Init(fd_list,sizeof(fd_list)/sizeof(int));

    for(;;){
        int max_fd = listen_fd;
        Reload(listen_fd,fd_list,sizeof(fd_list)/sizeof(int),&read_fds,&max_fd);
        cout << "before select:" << FD_ISSET(listen_fd,&read_fds);
        int ret = select(max_fd+1,&read_fds,NULL,NULL,NULL);
        cout << "after select:" << FD_ISSET(listen_fd,&read_fds);
        if(ret < 0){
            perror("select");
            continue;
        }
        if(ret == 0){
            cout << "select timeout" <<endl;
            continue;
        }

        //处理listen_fd
        if(FD_ISSET(listen_fd,&read_fds)){
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int connect_fd = accept(listen_fd,(struct sockaddr*)&client_addr,&len);
            if(connect_fd < 0){
                perror("accept error");
                continue;
            }
            cout <<"client" << inet_ntoa(client_addr.sin_addr)<< ":"
                << ntohs(client_addr.sin_port)<<endl;
            Add(connect_fd,fd_list,sizeof(fd_list)/sizeof(int));
        }

        //处理connect_fd
        for(size_t i=0;i<sizeof(fd_list)/sizeof(int);++i)
        {
            if(fd_list[i] == -1){
                continue;
            }
            if(!FD_ISSET(fd_list[i],&read_fds)){
                continue;
            }

            char buf[1024] = {0};
            ssize_t read_size = read(fd_list[i],buf,sizeof(buf)-1);
            if(read_size < 0){
                perror("read reeor");
                continue;
            }
            if(read_size == 0){
                cout <<"client say :goodbye"<< endl;
                close(fd_list[i]);
                fd_list[i] = -1;
            }
            cout <<"client say:"<< buf <<endl;
            write(fd_list[i] ,buf,strlen(buf));
        }
    }
    return 0;
}
