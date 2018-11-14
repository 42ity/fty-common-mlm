# fty-common-mlm

This is a library providing :
* Common methods and functions that require use of cxxtools/Malamute

## How to build

To build the `fty-common-mlm` project run:

```bash
./autogen.sh [clean]
./configure
make
make check # to run self-test
```

## List of Available Headers
* fty\_common\_mlm.h
* fty\_common\_mlm\_guards.h
* fty\_common\_rest\_sasl.h
* fty\_common\_mlm\_subprocess.h
* fty\_common\_mlm\_tntmlm.h
* fty\_common\_mlm\_utils.h
* fty\_common\_mlm\_uuid.h

## How to compile and test projects using fty-common-mlm by 42ITy standards

### project.xml
Add this block in the `project.xml` file :

````
<use project = "fty-common-mlm" libname = "libfty_common_mlm" header = "fty_common_mlm.h"
    repository = "https://github.com/42ity/fty-common-mlm.git"
    release = "master"
    test = "fty_commmon_mlm_selftest" >

    <use project = "czmq"
        repository = "https://github.com/42ity/czmq.git"
        release = "v3.0.2-FTY-master"
        min_major = "3" min_minor = "0" min_patch = "2" >

        <use project = "libzmq"
            repository = "https://github.com/42ity/libzmq.git"
            release = "4.2.0-FTY-master" >

            <use project = "libsodium" prefix = "sodium"
                repository = "https://github.com/42ity/libsodium.git"
                release = "1.0.5-FTY-master"
                test = "sodium_init"
                />
        </use>
    </use>

    <use project = "cxxtools"
        test = "cxxtools::Utf8Codec::Utf8Codec"
        header = "cxxtools/allocator.h"
        repository = "https://github.com/42ity/cxxtools.git"
        release = "2.2-FTY-master"
        />

    <use project = "malamute" min_major = "1" test = "mlm_server_test"
        repository = "https://github.com/42ity/malamute.git"
        release = "1.0-FTY-master"
        />

    <use project = "openssl" header = "openssl/sha.h" debian_name = "libssl-dev"/>

    <use project = "fty-common" libname = "libfty_common" header = "fty_common.h"
        repository = "https://github.com/42ity/fty-common.git"
        release = "master"
        test = "fty_common_selftest" >

        <use project = "fty-common-logging" libname = "libfty_common_logging" header = "fty_log.h"
            repository = "https://github.com/42ity/fty-common-logging.git"
            release = "master"
            test = "fty_common_logging_selftest" >

            <use project = "log4cplus" header = "log4cplus/logger.h" test = "appender_test"
                repository = "https://github.com/42ity/log4cplus.git"
                release = "1.1.2-FTY-master"
                />
        </use>
    </use>

    <use project = "fty-common-logging" libname = "libfty_common_logging" header = "fty_log.h"
        repository = "https://github.com/42ity/fty-common-logging.git"
        release = "master"
        test = "fty_common_logging_selftest" >

        <use project = "log4cplus" header = "log4cplus/logger.h" test = "appender_test"
            repository = "https://github.com/42ity/log4cplus.git"
            release = "1.1.2-FTY-master"
            />
    </use>
</use>
````
