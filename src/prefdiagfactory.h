// ライセンス: GPL2

//
// SKELETON::PrefDiagのファクトリー
//

#ifndef _PREFDIAGFACTORY_H
#define _PREFDIAGFACTORY_H

#include "skeleton/prefdiag.h"

#include <gtkmm.h>
#include <string>

namespace CORE
{
    enum
    {
        PREFDIAG_PASSWD,
        PREFDIAG_BROWSER,
        PREFDIAG_PROXY,
        PREFDIAG_GLOBALABONETHREAD,
        PREFDIAG_GLOBALABONE,
        PREFDIAG_FONTCOLOR,
        PREFDIAG_BOARDITEM,

        PREFDIAG_DELIMG,

        PREFDIAG_BOARD,
        PREFDIAG_ARTICLE,
        PREFDIAG_IMAGE,

        PREFDIAG_NONE
    };
    
    SKELETON::PrefDiag* PrefDiagFactory( Gtk::Window* parent, int type, const std::string& url );
}

#endif
