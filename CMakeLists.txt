cmake_minimum_required(VERSION 3.10.0)
project(BBScript VERSION 0.1.0 LANGUAGES C CXX)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)


# Пути генерируемых файлов в исходном каталоге
set(GRAMMAR_C ${CMAKE_CURRENT_SOURCE_DIR}/gramsrc/grammar.tab.c)
set(GRAMMAR_H ${CMAKE_CURRENT_SOURCE_DIR}/gramsrc/grammary.h)
set(LEXER_C   ${CMAKE_CURRENT_SOURCE_DIR}/gramsrc/grammar.yy.c)


FLEX_TARGET(Lexer gramsrc/grammarl.l ${LEXER_C})
BISON_TARGET(Parser gramsrc/grammary.y ${GRAMMAR_C} DEFINES_FILE ${GRAMMAR_H} )
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)


set(FIRDPARTY_DIR C:/Projects/BBRobot/ThirdParty)


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/gramsrc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/robstride)

#serial
include_directories(${FIRDPARTY_DIR}/include)
set(SERIAL_SRC ${FIRDPARTY_DIR}/serial/src)

#SLCAN
set(SLCAN_SRC ${FIRDPARTY_DIR}/SLCAN/Sources)
include_directories(${SLCAN_SRC})

#rapidxml
set(RAPIDXML_SRC ${FIRDPARTY_DIR}/rapidxml)
include_directories(${RAPIDXML_SRC})

#FLTK Укажите реальные пути к include и lib вашей локальной сборки FLTK
set(FLTK_INCLUDE_DIR ${FIRDPARTY_DIR}/fltk)
set(FLTK_LIBRARY_DIR ${FIRDPARTY_DIR}/fltk/build/lib)
include_directories(${FLTK_INCLUDE_DIR})
include_directories(${FIRDPARTY_DIR}/fltk/build)
#копируем конфиг

add_executable(main
    ${FLEX_Lexer_OUTPUTS}
    ${BISON_Parser_OUTPUTS}
    mainGUI.cpp
    macroses.cpp
    mainwnd.cpp
    mrobotimpl.cpp
    platform_util.cpp
    motors_facoty.cpp
    pcanimpl.cpp
    canmotorimpl.cpp
    tracerecoder.cpp
    feedbackhandler.cpp
    ak_can_motor.cpp
# CAN Library
    ${SLCAN_SRC}/slcan.c
    ${SLCAN_SRC}/serial.c
    ${SLCAN_SRC}/timer.c
    ${SLCAN_SRC}/queue.c
    ${SLCAN_SRC}/logger.c
    ${SLCAN_SRC}/buffer.c
# CAN  for Robstride
    robstride/Robstride.cpp
# CAN  and UART interfaces for Odrive
    motor_can_odrv.cpp
    motor_uart_odrv.cpp
 #   odrive/ODriveCAN.cpp
    odrive/ODriveUART.cpp
# Serial port library
    ${SERIAL_SRC}/serial.cc
    ${SERIAL_SRC}/impl/win.cc
# Grammar source files
    script_bind.cpp
    builtin_robot.c        
    gramsrc/bison_utils.c
    gramsrc/builtin.c
    gramsrc/checkarg.c
    gramsrc/interpreter.c
    gramsrc/utils.c
    gramsrc/value.c
    gramsrc/variable.c
)



target_link_libraries(main
    ${FLTK_LIBRARY_DIR}/libfltk.a
    ${FLTK_LIBRARY_DIR}/libfltk_images.a
    ${FLTK_LIBRARY_DIR}/libfltk_jpeg.a
    ${FLTK_LIBRARY_DIR}/libfltk_png.a
#    ${FLTK_LIBRARY_DIR}/libfltk_forms.a
    ${FLTK_LIBRARY_DIR}/libfltk_gl.a
    opengl32
    gdi32
    dwmapi
    ws2_32
    gdiplus
    comctl32
)

add_custom_command(
  TARGET main POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_SOURCE_DIR}/config.xml
          $<TARGET_FILE_DIR:main>/config.xml
)