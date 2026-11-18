cmake_minimum_required(VERSION 3.16)
project(TuxSteward VERSION 0.7.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets)
find_package(Threads REQUIRED)
add_library(tux-autostart src/autostart.cpp)
target_include_directories(tux-autostart PUBLIC include)
target_link_libraries(tux-autostart PUBLIC Qt5::Core)
target_compile_options(tux-autostart PRIVATE -Wall -Wextra -Wpedantic)
add_library(tux-core src/core.cpp src/runner.cpp)
target_include_directories(tux-core PUBLIC include)
target_link_libraries(tux-core PUBLIC Qt5::Core Threads::Threads)
target_compile_options(tux-core PRIVATE -Wall -Wextra -Wpedantic)
add_library(tux-ui src/window.cpp)
target_link_libraries(tux-ui PUBLIC tux-core Qt5::Widgets)
target_compile_options(tux-ui PRIVATE -Wall -Wextra -Wpedantic)
add_executable(tux-steward src/main.cpp)
target_link_libraries(tux-steward PRIVATE tux-ui)
add_executable(tux-task-manager src/task_manager_main.cpp src/process_manager.cpp include/tux/process_manager.hpp)
target_include_directories(tux-task-manager PRIVATE include)
target_link_libraries(tux-task-manager PRIVATE Qt5::Widgets tux-autostart)
target_compile_options(tux-task-manager PRIVATE -Wall -Wextra -Wpedantic)
include(CTest)
if(BUILD_TESTING)
    add_executable(tux-tests tests/test_core.cpp)
    target_link_libraries(tux-tests PRIVATE tux-core)
    add_test(NAME core COMMAND tux-tests)
    set_tests_properties(core PROPERTIES TIMEOUT 30)
    add_executable(tux-ui-tests tests/test_ui.cpp)
    target_link_libraries(tux-ui-tests PRIVATE tux-ui)
    add_test(NAME ui COMMAND tux-ui-tests)
    set_tests_properties(ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)

    add_executable(tux-task-manager-tests
        tests/test_task_manager.cpp
        src/process_manager.cpp
        include/tux/process_manager.hpp
        include/tux/system_monitor_tabs.hpp
    )
    target_include_directories(tux-task-manager-tests PRIVATE include)
    target_link_libraries(tux-task-manager-tests PRIVATE Qt5::Widgets tux-autostart)
    target_compile_options(tux-task-manager-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME task-manager-tabs COMMAND tux-task-manager-tests)
    set_tests_properties(task-manager-tabs PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 20
    )
    add_executable(tux-process-explorer-tests
        tests/test_process_explorer.cpp
        src/process_manager.cpp
    )
    target_include_directories(tux-process-explorer-tests PRIVATE include)
    target_link_libraries(tux-process-explorer-tests PRIVATE Qt5::Widgets tux-autostart)
    target_compile_options(tux-process-explorer-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME process-explorer COMMAND tux-process-explorer-tests)
    set_tests_properties(process-explorer PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
    )
    add_executable(tux-service-query-tests tests/test_service_queries.cpp)
    target_include_directories(tux-service-query-tests PRIVATE include)
    target_link_libraries(tux-service-query-tests PRIVATE Qt5::Widgets tux-autostart)
    target_compile_options(tux-service-query-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME service-queries COMMAND tux-service-query-tests)
    set_tests_properties(service-queries PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
    )
    add_executable(tux-autostart-tests tests/test_autostart.cpp)
    target_include_directories(tux-autostart-tests PRIVATE include)
    target_link_libraries(tux-autostart-tests PRIVATE Qt5::Widgets tux-autostart)
    target_compile_options(tux-autostart-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME autostart COMMAND tux-autostart-tests)
    set_tests_properties(autostart PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
endif()
include(GNUInstallDirs)
install(TARGETS tux-steward RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS tux-task-manager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/tux-steward.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/tux-task-manager.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES LICENSE PRIVACY.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/tux-steward)

