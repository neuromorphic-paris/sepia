if _ACTION == 'uninstall' then
    -- Uninstall for Linux
    if os.is('linux') then
        os.execute('sudo rm -f /usr/lib/libudev.so.0 && sudo rm -f /usr/lib/libudev.so.0.13.0')
        os.execute('rm -f ' .. path.join(prefix, 'include/opalkellyfrontpanel.h'))
        os.execute('rm -f ' .. path.join(prefix, 'lib/libopalkellyfrontpanel.so'))
        os.execute('sudo rm -f /etc/udev/rules.d/opalkelly.rules && sudo udevadm control --reload-rules')

    -- Uninstall for Mac OS X
    elseif os.is('macosx') then
        os.execute('rm -f ' .. path.join(prefix, 'include/opalkellyfrontpanel.h'))
        os.execute('rm -f ' .. path.join(prefix, 'lib/libopalkellyfrontpanel.dylib'))
    end
else
    local download = function(localPrefix, filename, sudo)
        if os.isfile(path.join(localPrefix, filename)) then
            return 0
        else
            return os.execute(
                (sudo and 'sudo' or '')
                .. ' wget -q -P '
                .. localPrefix
                .. ' '
                .. path.join('134.157.180.144:3002/opalKellyFrontPanel/', filename)
            )
        end
    end

    -- Installation under Linux
    if (os.is('linux')) then
        -- @DEV Check wether the linux os is 64 bits. Starting from premake 4.4, the method os.is64bit should be used instead.
        local pipe = io.popen('uname -m')
        local result = pipe:read('*a')
        pipe:close()

        --  Installation for 64 bits platforms
        if string.sub(result, 1, string.len('x86_64')) == 'x86_64' then
            if download('/usr/lib', 'libudev.x86_64.so.0.13.0', true) ~= 0 then
                print(
                    string.char(27)
                    .. '[31mOpalKellyFrontPanel download failed. Make sure that you are connected to the Vision Institute local network.'
                    .. string.char(27)
                    .. '[0m'
                )
                os.exit()
            end
            os.execute('sudo ln -s -f /usr/lib/libudev.so.0.13.0 /usr/lib/libudev.so.0')
            if download(path.join(prefix, 'include'), 'opalkellyfrontpanel.h', false) ~= 0 then
                print(
                    string.char(27)
                    .. '[31mOpalKellyFrontPanel download failed. Make sure that you are connected to the Vision Institute local network.'
                    .. string.char(27)
                    .. '[0m'
                )
                os.exit()
            end
            if download(path.join(prefix, 'lib'), 'libopalkellyfrontpanel.x86_64.so', false) ~= 0 then
                print(
                    string.char(27)
                    .. '[31mOpalKellyFrontPanel download failed. Make sure that you are connected to the Vision Institute local network.'
                    .. string.char(27)
                    .. '[0m'
                )
                os.exit()
            end
            print(string.char(27) .. '[32mOpalKellyFrontPanel installed.' .. string.char(27) .. '[0m')

        -- Installation for 32 bits platforms
        elseif string.sub(result, 1, string.len('i686')) == 'i686' then
            if download('/usr/lib', 'libudev.i686.so.0.13.0', true) ~= 0 then
                print(
                    string.char(27)
                    .. '[31mOpalKellyFrontPanel download failed. Make sure that you are connected to the Vision Institute local network.'
                    .. string.char(27)
                    .. '[0m'
                )
                os.exit()
            end
            os.execute('sudo ln -s -f /usr/lib/libudev.so.0.13.0 /usr/lib/libudev.so.0')
            if download(path.join(prefix, 'include'), 'opalkellyfrontpanel.h', false) ~= 0 then
                print(
                    string.char(27)
                    .. '[31mOpalKellyFrontPanel download failed. Make sure that you are connected to the Vision Institute local network.'
                    .. string.char(27)
                    .. '[0m'
                )
                os.exit()
            end
            if download(path.join(prefix, 'lib'), 'libopalkellyfrontpanel.i686.so', false) ~= 0 then
                print(
                    string.char(27)
                    .. '[31mOpalKellyFrontPanel download failed. Make sure that you are connected to the Vision Institute local network.'
                    .. string.char(27)
                    .. '[0m'
                )
                os.exit()
            end
            print(string.char(27) .. '[32mOpalKellyFrontPanel installed.' .. string.char(27) .. '[0m')
        end

        -- Copy the udev-rules, required for non-superuser access to the device
        if download('/etc/udev/rules.d', 'opalkelly.rules', true) ~= 0 then
            print(
                string.char(27)
                .. '[31mOpalKellyFrontPanel download failed. Make sure that you are connected to the Vision Institute local network.'
                .. string.char(27)
                .. '[0m'
            )
            os.exit()
        end
        os.execute('sudo udevadm control --reload-rules')

    -- Installation under Mac OS X
    elseif (os.is('macosx')) then
        if download(path.join(prefix, 'include'), 'opalkellyfrontpanel.h', false) ~= 0 then
            print(
                string.char(27)
                .. '[31mOpalKellyFrontPanel download failed. Make sure that you are connected to the Vision Institute local network.'
                .. string.char(27)
                .. '[0m'
            )
            os.exit()
        end
        if download(path.join(prefix, 'lib'), 'libopalkellyfrontpanel.dylib', false) ~= 0 then
            print(
                string.char(27)
                .. '[31mOpalKellyFrontPanel download failed. Make sure that you are connected to the Vision Institute local network.'
                .. string.char(27)
                .. '[0m'
            )
            os.exit()
        end
        print(string.char(27) .. '[32mOpalKellyFrontPanel installed.' .. string.char(27) .. '[0m')
    end
end
