if _ACTION == 'uninstall' then
    os.rmdir(path.join(prefix, 'share/sepia'))
else
    local download = function(localPrefix, filename)
        if os.isfile(path.join(localPrefix, filename)) then
            return 0
        else
            return os.execute(
                'wget -q -P '
                .. localPrefix
                .. ' '
                .. path.join('134.157.180.144:3002/firmwares/', filename)
            )
        end
    end

    os.mkdir(path.join(prefix, 'share/sepia'))
    if download(path.join(prefix, 'share/sepia'), 'atis.1.1.1.bit') ~= 0 then
        print(
            string.char(27)
            .. '[31mFirmwares download failed. Make sure that you are connected to the Vision Institute local network.'
            .. string.char(27)
            .. '[0m'
        )
        os.exit()
    end
    print(string.char(27) .. '[32mFirmwares installed.' .. string.char(27) .. '[0m')
end
