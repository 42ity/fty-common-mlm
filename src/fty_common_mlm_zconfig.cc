/*  =========================================================================
    fty_common_mlm_zconfig - C++ Wrapper Class fro zconfig

    Copyright (C) 2014 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    fty_common_mlm_zconfig - C++ Wrapper Class fro zconfig
@discuss
@end
*/

#include "fty_common_mlm_classes.h"

#include <stdexcept>
#include <string>

namespace mlm
{
    ZConfig::ZConfig(const std::string & path)
        : m_ptrConfig(zconfig_load(path.c_str()))
    {
        //check that the file was read properly
        if (!m_ptrConfig)
        {
            throw std::runtime_error("Impossible to read the config file <"+path+">");
        }
    }

    ZConfig::~ZConfig()
    {
        zconfig_destroy(&m_ptrConfig);
    }

    std::string ZConfig::getEntry(const std::string & entry, const std::string defaultValue) const
    {
        return std::string(zconfig_get(m_ptrConfig, entry.c_str(), defaultValue.c_str()));
    }

    void ZConfig::setEntry(const std::string & entry, const std::string value)
    {
        zconfig_put(m_ptrConfig, entry.c_str(), value.c_str());
    }

    void ZConfig::save(const std::string & path)
    {
        int result = zconfig_save(m_ptrConfig, path.c_str());
        if(result != 0)
        {
            throw std::runtime_error("Impossible to save the config file <"+path+">, error: "+std::to_string(result));
        }
    }

} // namespace mlm

#define SELFTEST_DIR_RO "src/selftest-ro"
#define SELFTEST_DIR_RW "src/selftest-rw"

void
fty_common_mlm_zconfig_test (bool verbose)
{
    printf (" * fty_common_mlm_zconfig: ");

    //  @selftest

    //open a none existing file
    try
    {
        mlm::ZConfig(SELFTEST_DIR_RO"/do_not_exist.ghost");
        assert(false);
    }
    catch(std::runtime_error &)
    {}


    //open an existing file
    mlm::ZConfig config(SELFTEST_DIR_RO"/test.conf");

    assert(config.getEntry("server/timeout") == "10000");
    assert(config.getEntry("server/endpoint") == "ipc://@/malamute");
    assert(config.getEntry("log/config") == "/etc/fty/ftylog.cfg");
    assert(config.getEntry("data") == "lotOfData");

    assert(config.getEntry("none/existing/entry") == "");
    assert(config.getEntry("none/existing/entry", "default") == "default");

    config.setEntry("data", "lotOfMoreData");
    assert(config.getEntry("data") == "lotOfMoreData");

    config.setEntry("none/existing/entry", "lotOfMoreData");
    assert(config.getEntry("none/existing/entry") == "data");

    //  @end
    printf ("OK\n");
}
