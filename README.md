znmodule
========
nginx.conf:

location /path {

  mytest;
  
}

location /path {

  zupstream;
  
  connection_timeout 50000ms;
  
  send_timeout 50000ms;
  
  read_timeout 50000ms;
  
}

How to install:

cd nginx

sudo ./configure --prefix=/path/to/install --add-module=/path/to/dev/module && sudo make && sudo make install

========

nginx modules
