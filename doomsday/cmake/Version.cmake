set (DE_VERSION_MAJOR 3)
set (DE_VERSION_MINOR 0)
set (DE_VERSION_PATCH 0)

set (DE_VERSION ${DE_VERSION_MAJOR}.${DE_VERSION_MINOR}.${DE_VERSION_PATCH})

# Binary compatibility version for shared libraries / APIs.
set (DE_COMPAT_VERSION 2.2)

string (REPLACE . , DE_VERSION_WINDOWS "${DE_VERSION}.${DE_BUILD}")
if (NOT DEFINED DE_BUILD)
    set (DE_VERSION_WINDOWS "${DE_VERSION_WINDOWS}0")
endif ()

set (DE_RELEASE_TYPE
    Unstable
    #Candidate
    #Stable
)

set (DE_TEAM_COPYRIGHT "Copyright (c) 2003-2019 Deng Team")

# Build Configuration --------------------------------------------------------

if (DE_RELEASE_TYPE STREQUAL "Stable")
    add_definitions (-DDE_STABLE=1)
    set (DE_STABLE 1)
endif ()
if (DEFINED DE_BUILD)
    add_definitions (-DDOOMSDAY_BUILD_TEXT="${DE_BUILD}")
endif ()
