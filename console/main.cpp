#include "CBC.h"
#include <QDebug>

int main(int argc, char* argv[])
{
    if(argc < 2) {
        qWarning() << argv[0] << " [c_file]";
        return 0;
    }
    CBC cbc;
    cbc.download(argv[1]);
    return 0;
}
