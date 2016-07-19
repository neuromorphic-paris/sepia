-- Define the install prefix
prefix = nil
    -- Installation under Linux
    if (os.is('linux')) then
        prefix = '/usr/local'
        os.execute('sudo chown -R `whoami` ' .. prefix .. ' && sudo chmod -R 751 ' .. prefix)

    -- Installation under Mac OS X
    elseif (os.is('macosx')) then
        prefix = '/usr/local'

    -- Other platforms
    else
        print(string.char(27) .. '[31mThe installation is not supported on your platform' .. string.char(27) .. '[0m')
        os.exit()
    end


-- Handle the icpc option
if _OPTIONS['icc'] then
    if _ACTION ~= 'gmake' then
        print(string.char(27) .. '[31mThe \'icc\' option is only supported for the \'gmake\' action' .. string.char(27) .. '[0m')
        os.exit()
    else
        premake.gcc.cc = 'icc'
        premake.gcc.cxx = 'icpc'
        print(string.char(27) .. '[32mThe makefile will be generated for the Intel compiler.' .. string.char(27) .. '[0m')
        if tonumber(string.match(_PREMAKE_VERSION, '%d%.%d+')) < 4.4 then
            print(
                string.char(27)
                .. '[33mYou need to run make with the \'-R\' to use the Intel c++ compiler. Upgrading to premake 4.4 solves this issue.'
                .. string.char(27)
                .. '[0m'
            )
        end
    end
end


solution 'sepia'
    configurations {'Release', 'Debug'}
    location 'build'

    newaction {
        trigger = "install",
        description = "Install the library",
        execute = function()
            os.copyfile('source/sepia.hpp', path.join(prefix, 'include/sepia.hpp'))
            os.copyfile('source/opalKellyAtisSepia.hpp', path.join(prefix, 'include/opalKellyAtisSepia.hpp'))
            print(string.char(27) .. '[32mSepia library installed.' .. string.char(27) .. '[0m')
            os.exit()
        end
    }

    newaction {
        trigger = 'uninstall',
        description = 'Remove all the files installed during build processes',
        execute = function()
            include 'resources/opalKellyFrontPanel'
            include 'resources/firmwares'
            os.execute('rm -f ' .. path.join(prefix, 'include/sepia.hpp'))
            os.execute('rm -f ' .. path.join(prefix, 'include/opalKellyAtisSepia.hpp'))
            print(string.char(27) .. '[32mSepia library uninstalled.' .. string.char(27) .. '[0m')
            os.exit()
        end
    }

    newoption {
        trigger = 'icc',
        description = 'Use the Intel c++ compiler with make'
    }

    newoption {
        trigger = 'skip-dependencies',
        description = 'Do not install the library dependencies'
    }

    project 'sepiaTest'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/**.hpp', 'test/**.hpp', 'test/**.cpp'}

        -- Install the dependencies if required
        if not _OPTIONS['skip-dependencies'] then
            include 'resources/opalKellyFrontPanel'
            include 'resources/firmwares'
        end

        -- Define the include paths
        includedirs {path.join(prefix, 'include')}
        libdirs {path.join(prefix, 'lib')}

        -- Link the dependencies
        links {'opalkellyfrontpanel'}

        -- Declare the configurations
        configuration 'Release'
            targetdir 'build/Release'
            defines {'NDEBUG'}
            flags {'OptimizeSpeed'}
        configuration 'Debug'
            targetdir 'build/Debug'
            defines {'DEBUG'}
            flags {'Symbols'}

        -- Linux specific settings
        configuration 'linux'
            buildoptions {'-std=c++11'}
            linkoptions {'-std=c++11'}
            links {'pthread'}
            postbuildcommands {
                'cp ../source/sepia.hpp ' .. path.join(prefix, 'include/sepia.hpp'),
                'cp ../source/opalKellyAtisSepia.hpp ' .. path.join(prefix, 'include/opalKellyAtisSepia.hpp')
            }

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}
            postbuildcommands {
                'cp ../source/sepia.hpp ' .. path.join(prefix, 'include/sepia.hpp'),
                'cp ../source/opalKellyAtisSepia.hpp ' .. path.join(prefix, 'include/opalKellyAtisSepia.hpp')
            }
