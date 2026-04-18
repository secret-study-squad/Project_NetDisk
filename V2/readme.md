- 实现断点续传

  - 客户端向服务端传递实现断点续传
  - 客户端下载文件没有实现断点续传

- 实现利用Linux系统用户进行登入

- ~~~c
  //服务端运行
  // sudo ./main  conf
  ~~~

- ~~~c
  //客户端运行
  //编译
  //gcc *.c -lcrypt
  //./a.out  ip   port
  ~~~

- 实现命令pwd/cd/ls/mkdir/rmdir/puts/gets

  - ls/mkdir/rmdir，只能在当前目录下面执行

- 大文件和小文件都使用同样的方式传输，没有实现大文件的零拷贝传输