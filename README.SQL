Hi,

Please edit hxd.conf and change the sql {} params for your setup:

then 

mysql -uuser -ppass database < hxd.sql

compile hxd with --enable-sql .. and it should work. check the text log file for errors.
