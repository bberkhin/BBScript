#include <vector>
#include <string>
#include <rapidxml.hpp>

    /* 
    <macros>
        <macro label="Vel control">
            setModeVel(-1);
            closeLoop(=1);
        </macro>
        <macro label="GoTo Zero">
            setModePos(-1);
            closeLoop(-1);
            moveTo([0,0,0,0,0,0]);
        </macro>

    </macros>
    */
struct Macro {
    std::string name;
    std::string code;
};

class Macroses {
public:
    Macroses() = default;
    ~Macroses() = default;
    void Load(rapidxml::xml_node<>* macros_node);    
    const std::vector<Macro>& GetMacros() const {
        return macros_;
    }
    static Macroses &getInstance();
private:
    std::string trim(const std::string& s);
private:
    std::vector<Macro> macros_;
};


