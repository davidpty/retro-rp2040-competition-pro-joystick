function(embed_default_ini source_dir output_dir)
    file(READ "${source_dir}/default_joystick.ini" default_ini_text)
    string(REPLACE "\\" "\\\\" default_ini_text "${default_ini_text}")
    string(REPLACE "\"" "\\\"" default_ini_text "${default_ini_text}")
    string(REPLACE "\n" "\\n\"\n\"" default_ini_text "${default_ini_text}")
    set(DEFAULT_INI_C_STRING "\"${default_ini_text}\"")
    configure_file(
        "${source_dir}/cmake/default_joystick_ini.h.in"
        "${output_dir}/default_joystick_ini.h"
        @ONLY
    )
endfunction()
