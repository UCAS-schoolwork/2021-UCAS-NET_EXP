#!/bin/bash
exp1(){
str1='h1 echo "nameserver 8.8.8.8" > /etc/resolv.conf\n'\
'h1 wget www.baidu.com\n'
echo -e ${str1} | sudo mn --nat 
}


exp1







