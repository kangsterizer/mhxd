CREATE TABLE start (
  
  id int(11) DEFAULT '0' NOT NULL auto_increment,
  date datetime,
  version varchar(10),
  PRIMARY KEY (id)

);

CREATE TABLE exec (

  id int(11) DEFAULT '0' NOT NULL auto_increment,
  date datetime,
  nick varchar(64),
  login varchar(64),
  ipaddr varchar(20),
  cmdpath text,
  thisarg varchar(32),
  PRIMARY KEY (id)

);

CREATE TABLE kick (

  id int(11) DEFAULT '0' NOT NULL auto_increment,
  date datetime,
  nick varchar(64),
  ipaddr varchar(20),
  login varchar(64),
	knick varchar(64),
	klogin varchar(64),
  PRIMARY KEY (id)

);

CREATE TABLE ban (

  id int(12) DEFAULT '0' NOT NULL auto_increment,
  date datetime,
  nick varchar(64),
  ipaddr varchar(20),
  login varchar(64),
	bnick varchar(64),
  blogin varchar(64),
  PRIMARY KEY (id)
  
);

CREATE TABLE connections (
  
  id int(11) DEFAULT '0' NOT NULL auto_increment,
  date datetime,
	userid text,
  nick varchar(64),
  ipaddr varchar(20),
	port int(11),
  login varchar(64),
  uid int(11),
  PRIMARY KEY (id)

);

CREATE TABLE disconnections (

  id int(11) DEFAULT '0' NOT NULL auto_increment,
  date datetime,
  userid text,
  nick varchar(64),
  ipaddr varchar(20),
  port int(11),
  login varchar(64),
  uid int(11),
  PRIMARY KEY (id)
  
);

CREATE TABLE download (

  id int(11) DEFAULT '0' NOT NULL auto_increment,
  date datetime,
  nick varchar(64),
  ipaddr varchar(20),
  login varchar(64),
  path text,
  PRIMARY KEY (id)

);

CREATE TABLE upload (

  id int(11) DEFAULT '0' NOT NULL auto_increment,
  date datetime,
  nick varchar(64),
  ipaddr varchar(20),
  login varchar(64),
  path text,
  PRIMARY KEY (id) 
  
);

CREATE TABLE user (

  date datetime,
  socket int(11),
  account varchar(64),
  ipaddr varchar(20),
  nickname varchar(64),
  icon int(6),
  color int(2)

);
