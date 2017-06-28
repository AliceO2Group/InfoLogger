#! /bin/sh

# This script configures infoLogger MySQL DB from scratch
# sylvain.chapeland@cern.ch

echo "Configuration of Mysql database for infoLogger"
echo "Please follow instructions. Values in [] are defaults if nothing answered"
echo ""


# Ask which MySQL server to use
read -p "Enter MySQL server host name [localhost] : " MYSQL_HOST
if [ "$MYSQL_HOST" = "" ]; then MYSQL_HOST=localhost; fi


# Test if a root password is defined
PWD=""
mysql -h $MYSQL_HOST -u root -e "exit" > /dev/null 2>&1
if [ "$?" = "0" ]; then 
  echo "No password is defined yet to access MySQL server with user 'root' on $MYSQL_HOST"
  stty -echo
  read -p "Enter new root password for mysql [leave blank]: " PWD
  stty echo
  echo

  if [ "$PWD" != "" ]; then
    stty -echo
    read -p "Enter again: " PWD2
    stty echo
    echo
    if [ "$PWD" != "$PWD2" ]; then
      echo "Mismatch!"
      exit
    fi
    /usr/bin/mysqladmin -h $MYSQL_HOST -u root password "$PWD"
    echo "Password updated"
    PWD="-p$PWD"
    # remove empty entries as well
    mysql -h $MYSQL_HOST -u root $PWD -e "DELETE FROM mysql.user WHERE User = ''; \
      FLUSH PRIVILEGES;" 2>/dev/null
  else
    echo "Root password left blank"
  fi

else
  stty -echo
  read -p "Enter root password for mysql : " PWD
  stty echo
  echo
  if [ "$PWD" != "" ]; then
    PWD="-p$PWD"
  fi

fi

# try connection
mysql -h $MYSQL_HOST -u root $PWD -e "exit" 2>/dev/null
if [ "$?" != "0" ]; then 
  echo "Connection failed"
  exit
fi

echo "Database creation - existing databases will NOT be destroyed"

# Create database
read -p "Enter a database name for infoLogger logs [INFOLOGGER] : " INFOLOGGER_MYSQL_DB
if [ "$INFOLOGGER_MYSQL_DB" = "" ]; then  INFOLOGGER_MYSQL_DB=INFOLOGGER; fi
mysql -h $MYSQL_HOST -u root $PWD -e "create database $INFOLOGGER_MYSQL_DB" 2>/dev/null



# random password generator
function createPwd {
  echo `< /dev/urandom tr -dc A-Za-z0-9 | head -c8`
}



# Create accounts
declare -a EXTRA_CONFIG=(infoLoggerServer infoBrowser admin);

declare -A EXTRA_USER
EXTRA_USER[infoLoggerServer]="infoLoggerServer"
EXTRA_USER[infoBrowser]="infoBrowser"
EXTRA_USER[admin]="infoLoggerAdmin"

declare -A EXTRA_PWD
EXTRA_PWD[infoLoggerServer]=$(createPwd)
EXTRA_PWD[infoBrowser]=$(createPwd)
EXTRA_PWD[admin]=$(createPwd)

declare -A EXTRA_PRIVILEGE
EXTRA_PRIVILEGE[infoLoggerServer]="insert"
EXTRA_PRIVILEGE[infoBrowser]="select"
EXTRA_PRIVILEGE[admin]="all privileges"

MYSQL_COMMANDS=""

for CONFIG in "${EXTRA_CONFIG[@]}"; do
  MYSQL_COMMAND=`echo "grant ${EXTRA_PRIVILEGE[$CONFIG]} on $INFOLOGGER_MYSQL_DB.* to \"${EXTRA_USER[$CONFIG]}\"@\"%\" identified by \"${EXTRA_PWD[$CONFIG]}\";"`
  MYSQL_COMMAND=`echo "grant ${EXTRA_PRIVILEGE[$CONFIG]} on $INFOLOGGER_MYSQL_DB.* to \"${EXTRA_USER[$CONFIG]}\"@\"localhost\" identified by \"${EXTRA_PWD[$CONFIG]}\";"`
  MYSQL_COMMANDS=${MYSQL_COMMANDS}$'\n'${MYSQL_COMMAND}
done
mysql -h $MYSQL_HOST -u root $PWD -e "$MYSQL_COMMANDS"

echo ""
echo "MySQL server accounts/db created"
echo ""
echo "You may use the following in the infoLogger config files:"
HERE=`hostname -f`
for CONFIG in "${EXTRA_CONFIG[@]}"; do
  echo "[$CONFIG]"
  echo "dbUser=${EXTRA_USER[$CONFIG]}"
  echo "dbPwd=${EXTRA_PWD[$CONFIG]}"
  echo "dbHost=$HERE"
  echo "dbName=$INFOLOGGER_MYSQL_DB"
  echo ""
done
