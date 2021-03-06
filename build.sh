#!/bin/bash
# what even is a computer
export C_INCLUDE_PATH=$(taglib-config --cflags | sed 's/-I//')
export CPLUS_INCLUDE_PATH=$C_INCLUDE_PATH

php_ini=$( php --ini | grep "Loaded Configuration File:" | awk '{print $4}' )
extension_dir=$(php-config --extension-dir)
if [ "$1" != "--quick" ]; then
# .
    phpize --with-php-config=$php_ini   
    ./configure --with-php-config=`which php-config` --enable-debug --with-taglib
    if [[ $(grep "\-std=c++11" Makefile) = "" ]]; then
        sed -i.bak 's/\(^CXXFLAGS =\)/\1 -std=c++11/' Makefile
        if [[ $? = 1 ]]; then 
            sed -i .bak 's/\(^CXXFLAGS =\)/\1 -std=c++11/' Makefile
        fi
        rm Makefile.bak
    fi
fi

# maybe I *should* modify config.m4
# or maybe you should shut up
touch taglib.cpp

make 2> /tmp/q
build_output=$(cat /tmp/q)
if [ "$build_output" == "" ]; then 
    echo "Build succeeded, copying new module..."
    sudo cp modules/taglib.so $extension_dir 
    if [ "$2" != "--no-tests" ]; then
        echo "Running tests..."
        cd test
        php -f taglib-php-tests.php | less -r
        if [[ ${PIPESTATUS[0]} = 0 ]]; then
            echo -ne "\033[32mbuild: passing\033[0m (go write more tests)\n"
        else
            echo -ne "\033[31mBUILD: FAILING IT\033[0m\n"
        fi
    fi
    echo "You can run the tests manually with:"
    echo "cd test && php -f taglib-php-tests.php"
else
    less /tmp/q
    exit 1;
fi
