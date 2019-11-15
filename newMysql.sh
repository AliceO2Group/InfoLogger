#! /bin/sh

# This script configures infoLogger MySQL DB from scratch
# sylvain.chapeland@cern.ch



# definition of default variables
# they can be overridden when running in automated non-interactive mode
# option i: disable interactive mode
# option a: source a bash script (e.g. to override multiple variables at once)
# option s: evaluate associated argument (e.g. to override a value)
# option f: force re-do all actions
# option q: enable SQL debugging

# defaults assume this script is executed on infoLoggerServer host
# and that DB runs on same node (local connection to db)

IS_INTERACTIVE=1
SQL_ROOT_USER=root
SQL_ROOT_PWD=""
SQL_ROOT_HOST=localhost

# if set, force all actions even if they seem not necessary
FORCE_REDO=0

# if set, SQL queries & output printed
SQL_DEBUG=0

# where are we running now
HERE=`hostname -f`

# runtime location of infologger DB
INFOLOGGER_DB_HOST=$HERE

# definition of parameters to be configured for various infologger tasks
declare -a EXTRA_CONFIG=(server browser admin);

# section headers in output config file
declare -A EXTRA_CONFIGSECTION
EXTRA_CONFIGSECTION[server]="infoLoggerServer"
EXTRA_CONFIGSECTION[browser]="infoBrowser"
EXTRA_CONFIGSECTION[admin]="admin"

# mysql user name
declare -A EXTRA_USER
EXTRA_USER[server]="infoLoggerServer"
EXTRA_USER[browser]="infoBrowser"
EXTRA_USER[admin]="infoLoggerAdmin"
# mysql user pwd
declare -A EXTRA_PWD
EXTRA_PWD[server]=""
EXTRA_PWD[browser]=""
EXTRA_PWD[admin]=""
# mysql host
declare -A EXTRA_HOST
EXTRA_HOST[server]="localhost"
EXTRA_HOST[browser]="$INFOLOGGER_DB_HOST"
EXTRA_HOST[admin]="$INFOLOGGER_DB_HOST"
# mysql user privileges
declare -A EXTRA_PRIVILEGE
EXTRA_PRIVILEGE[server]="insert"
EXTRA_PRIVILEGE[browser]="select"
EXTRA_PRIVILEGE[admin]="all privileges"

# name of database
INFOLOGGER_DB_NAME=INFOLOGGER

# file where to put (if value is stdout, just print on screen)
INFOLOGGER_CONFIG=stdout

# random password generator
function createPwd {
  echo `< /dev/urandom tr -dc A-Za-z0-9 | head -c8`
}

# generate random passwords for all infologger users
for CONFIG in "${EXTRA_CONFIG[@]}"; do
  EXTRA_PWD[$CONFIG]=$(createPwd)
done

# an alias for mysql exe, easy to replace for dry run
MYSQL_EXE=mysql
#MYSQL_EXE=echo


# parse command line arguments
while getopts "ia:s:fq" option
do
  case $option in
    i)
      echo "Running non-interactive, automated mode selected"
      IS_INTERACTIVE=0    
      ;;
    a)    
      FN=$OPTARG
      if [ "$FN" != "" ]; then
        echo "Sourcing file '$FN'"
        if [ ! -f $FN ]; then
          echo "File not found"
          exit 1
        fi
        # load source file to set parameters requested interactively otherwise
        # see name of default variables above, for those needing to be changed
        source $FN
      fi
      ;;
    s)
      eval $OPTARG
      ;;
    f)
      echo "Force redo all actions"
      FORCE_REDO=1
      ;;
    q)
      echo "SQL debugging enabled"
      SQL_DEBUG=1
      ;;  
  esac
done


if [ "$IS_INTERACTIVE" -eq "1" ]; then
  # begin interactive part
  
  echo "Configuration of Mysql database for infoLogger"
  echo "Please follow instructions. Values in [] are defaults if nothing answered"
  echo ""

  # Ask which MySQL server to use
  read -p "Enter MySQL server host name [$SQL_ROOT_HOST] : " P_SQL_ROOT_HOST
  if [ "$P_SQL_ROOT_HOST" != "" ]; then SQL_ROOT_HOST=$P_SQL_ROOT_HOST; fi

  # Test if a SQL_ROOT_USER password is defined
  mysql -h $SQL_ROOT_HOST -u $SQL_ROOT_USER -e "exit" > /dev/null 2>&1
  if [ "$?" = "0" ]; then 
    echo "No password is defined yet to access MySQL server with mysql user '$SQL_ROOT_USER' on $SQL_ROOT_HOST"
    stty -echo
    read -p "Enter new password for mysql user '$SQL_ROOT_USER' [leave blank]: " SQL_ROOT_PWD
    stty echo
    echo

    if [ "$SQL_ROOT_PWD" != "" ]; then
      stty -echo
      read -p "Enter again: " SQL_ROOT_PWD2
      stty echo
      echo
      if [ "$SQL_ROOT_PWD" != "$SQL_ROOT_PWD2" ]; then
        echo "Mismatch!"
        exit 1
      fi
      /usr/bin/mysqladmin -h $SQL_ROOT_HOST -u $SQL_ROOT_USER password "$SQL_ROOT_PWD"
      echo "Password updated"
      # remove empty entries as well
      mysql -h $SQL_ROOT_HOST -u $SQL_ROOT_USER -p$SQL_ROOT_PWD -e "DELETE FROM mysql.user WHERE User = ''; \
        FLUSH PRIVILEGES;" 2>/dev/null
    else
      echo "mysql user '$SQL_ROOT_USER' password left blank"
    fi

  else
    stty -echo
    read -p "Enter password for mysql user '$SQL_ROOT_USER' : " SQL_ROOT_PWD
    stty echo
    echo
  fi

  read -p "Enter a database name for infoLogger logs [$INFOLOGGER_DB_NAME] : " P_INFOLOGGER_DB_NAME
  if [ "$P_INFOLOGGER_DB_NAME" != "" ]; then  INFOLOGGER_DB_NAME=$P_INFOLOGGER_DB_NAME; fi
    
  read -p "Enter a file name where to save infoLogger configuration [just print on screen] : " P_INFOLOGGER_CONFIG
  if [ "$P_INFOLOGGER_CONFIG" != "" ]; then  INFOLOGGER_CONFIG=$P_INFOLOGGER_CONFIG; fi

  # end interactive part
fi

# define command line password argument
if [ "$SQL_ROOT_PWD" != "" ]; then
  SQL_PWD_ARG="-p$SQL_ROOT_PWD"
fi

# try connection
mysql -h $SQL_ROOT_HOST -u $SQL_ROOT_USER $SQL_PWD_ARG -e "exit" 2>/dev/null
if [ "$?" != "0" ]; then 
  echo "MySQL connection failed"
  exit 1
fi

# function to execute SQL command
# arg1: SQL command
# arg2: database name, if any
function mysqlExecute {
  if [ "$SQL_DEBUG" -eq "1" ]; then
    echo "SQL : $1"
  fi
  SQLRES=`$MYSQL_EXE -h $SQL_ROOT_HOST -u $SQL_ROOT_USER $SQL_PWD_ARG -B -N -e "$1" $2 2>&1`
  RET=$?
  if [ "$SQL_DEBUG" -eq "1" ]; then
    if [ "$SQLRES" != "" ]; then
      echo "$SQLRES"
    fi
  fi
  return $RET
}

echo "Setting up mysql for infoLogger"

CHANGES=0

# Create database
mysqlExecute "quit" $INFOLOGGER_DB_NAME
if [ "$?" != "0" ] || [ "$FORCE_REDO" -eq 1 ]; then 
  echo "Create database $INFOLOGGER_DB_NAME"
  mysqlExecute "create database $INFOLOGGER_DB_NAME"
  if [ "$?" != "0" ] && [ "$FORCE_REDO" -eq 0 ]; then 
    exit
  fi
  CHANGES=1
else
  echo "Database $INFOLOGGER_DB_NAME already exists, skipping database create"
fi


# for mysql8
#PWDOPT="with mysql_native_password"

# Create accounts SQL command
MYSQL_COMMANDS=""

for CONFIG in "${EXTRA_CONFIG[@]}"; do
  # check if we can already connect
  mysql -u "${EXTRA_USER[$CONFIG]}" -p"${EXTRA_PWD[$CONFIG]}" -B -N -e "quit" $INFOLOGGER_DB_NAME > /dev/null 2>&1
#  mysqlExecute "select current_user()"
  
  if [ "$?" == "0" ] && [ "$FORCE_REDO" -eq 0 ]; then
    echo "Can already connect as ${EXTRA_USER[$CONFIG]}, skipping user create"
    continue
  else
    CHANGES=1
  fi
  
  for QHOST in "%" "localhost" "${HERE}"; do
    mysqlExecute "drop user \"${EXTRA_USER[$CONFIG]}\"@\"${QHOST}\";"
    mysqlExecute "create user \"${EXTRA_USER[$CONFIG]}\"@\"${QHOST}\";"
    mysqlExecute "set password for  \"${EXTRA_USER[$CONFIG]}\"@\"${QHOST}\" = PASSWORD(\"${EXTRA_PWD[$CONFIG]}\");"
    mysqlExecute "grant ${EXTRA_PRIVILEGE[$CONFIG]} on $INFOLOGGER_DB_NAME.* to \"${EXTRA_USER[$CONFIG]}\"@\"${QHOST}\";"
  done
done

echo "MySQL infoLogger accounts created"

if [ "$CHANGES" == "0" ]; then
  echo "No changes"
fi

# generate a sample configuration
INFOLOGGER_SAMPLE_CONFIG="# infoLogger configuration file"$'\n'$'\n'
for CONFIG in "${EXTRA_CONFIG[@]}"; do
  INFOLOGGER_SAMPLE_CONFIG+="[${EXTRA_CONFIGSECTION[$CONFIG]}]"$'\n'
  INFOLOGGER_SAMPLE_CONFIG+="dbUser=${EXTRA_USER[$CONFIG]}"$'\n'
  INFOLOGGER_SAMPLE_CONFIG+="dbPassword=${EXTRA_PWD[$CONFIG]}"$'\n'
  INFOLOGGER_SAMPLE_CONFIG+="dbHost=${EXTRA_HOST[$CONFIG]}"$'\n'
  INFOLOGGER_SAMPLE_CONFIG+="dbName=$INFOLOGGER_DB_NAME"$'\n'
  INFOLOGGER_SAMPLE_CONFIG+="serverHost=${HERE}"$'\n'
  INFOLOGGER_SAMPLE_CONFIG+=""$'\n'
done
  INFOLOGGER_SAMPLE_CONFIG+="[infoLoggerD]"$'\n'
  INFOLOGGER_SAMPLE_CONFIG+="serverHost=${HERE}"$'\n'

if [ "$INFOLOGGER_CONFIG" != "" ]; then  
  if [ "$INFOLOGGER_CONFIG" != "stdout" ]; then  
    echo "Sample configuration saved to $INFOLOGGER_CONFIG"
    echo "$INFOLOGGER_SAMPLE_CONFIG" > $INFOLOGGER_CONFIG
  else
    echo -e "You may use the following in the infoLogger config files:\n\n"
    echo "$INFOLOGGER_SAMPLE_CONFIG"
  fi
fi
