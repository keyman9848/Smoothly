#include "pathfinding.h"
namespace smoothly{

int pathFinding::pathFindingContext::heuristic(const building::blockPosition & p1, building::blockPosition & p2){
    //return abs(p1.x-p2.x) + abs(p1.y-p2.y) + abs(p1.z-p2.z);
    int x = p1.x-p2.x;
    int y = p1.y-p2.y;
    int z = p1.z-p2.z;
    return sqrt(x*x+y*y+z*z);
}

void pathFinding::pathFindingContext::getWalkablePoint(int x, int y,int z, std::vector<blockPosition> & res){
    checkWalkablePoint(x,y,z+1,res);
    checkWalkablePoint(x,y,z-1,res);
    checkWalkablePoint(x-1,y,z,res);
    checkWalkablePoint(x+1,y,z,res);

    checkWalkablePoint(x-1,y,z-1,res);
    checkWalkablePoint(x-1,y,z+1,res);
    checkWalkablePoint(x+1,y,z-1,res);
    checkWalkablePoint(x+1,y,z+1,res);
}

void pathFinding::pathFindingContext::checkWalkablePoint(int x, int y, int z, std::vector<blockPosition> & res){
    int h = parent->getCollHeight(x,z);
    bool b0 = parent->getCollMap(x,y-2,z);
    bool b1 = parent->getCollMap(x,y-1,z);
    bool b2 = parent->getCollMap(x,y  ,z);
    bool b3 = parent->getCollMap(x,y+1,z);
    //bool b4 = parent->getCollMap(x,y+2,z);

    if(y-2<=h)
        b0 = true;

    if(y-1<=h)
        b1 = true;

    if(y<=h)
        b2 = true;

    if(y+1<=h)
        b3 = true;

    //if(y+2<=h)
    //    b4 = true;

    if(b3==false && b2==true)
        res.push_back(blockPosition(x,y+1,z));
    else if(b2==false && b1==true)
        res.push_back(blockPosition(x,y,z));
    else if(b1==false && b0==true)
        res.push_back(blockPosition(x,y-1,z));
}

void pathFinding::pathFindingContext::init(){
    clear();
    auto st = new node;
    st->f=0;
    st->g=0;
    st->h=0;
    st->parent=NULL;
    st->position=start;
    processing = st;
    result = NULL;
    failed = false;
}

void pathFinding::pathFindingContext::process(){
    if(processing==NULL)
        return;
    std::vector<blockPosition> lst;
    getWalkablePoint(
                processing->position.x,
                processing->position.y,
                processing->position.z,
                lst);
    closelist[processing->position] = processing;

    std::vector<node*> ns;
    for(auto it:lst){
        if(openlist.find(it)!=openlist.end())
            continue;
        if(closelist.find(it)!=closelist.end())
            continue;
        auto p = new node;
        p->parent = processing;
        p->position = it;

        //启发
        p->g = processing->g + heuristic(p->position,it);
        p->h = heuristic(it,target);
        p->f = p->g + p->h;

        openlist[it] = p;
        ns.push_back(p);
    }
    if(ns.empty()){
        if(openlist.empty()){
            failed = true;//搜索失败
            processing = NULL;
            return;
        }else{
            int minf=-1;
            node * min;
            //openlist中找一个f最小的
            for(auto it:openlist){
                if(minf==-1){
                    minf = it.second->f;
                    min  = it.second;
                }else{
                    if(it.second->f < minf){
                        minf = it.second->f;
                        min  = it.second;
                    }
                }
            }
            processing = min;
            openlist.erase(min->position);
        }
    }else{
        int minf=-1;
        node * min;
        //ns中找一个f最小的
        for(auto it:ns){
            if(heuristic(it->position,target)<4){
                processing = NULL;
                result = it;
                return;
            }
            if(minf==-1){
                minf = it->f;
                min  = it;
            }else{
                if(it->f < minf){
                    minf = it->f;
                    min  = it;
                }
            }
        }
        processing = min;
        openlist.erase(min->position);
    }
}

void pathFinding::pathFindingContext::clear(){
    if(processing){
        delete processing;
        processing=NULL;
    }
    for(auto it:openlist){
        delete it.second;
    }
    openlist.clear();
    for(auto it:closelist){
        delete it.second;
    }
    closelist.clear();
}

void pathFinding::pathFindingContext::buildRoad(std::function<void(const building::blockPosition &)> callback){
    if(result){
        auto p = result;
        while(p){
            callback(p->position);
            p=p->parent;
        }
    }
}

bool pathFinding::pathFindingContext::pathFindingStart(std::function<void(const building::blockPosition &)> callback){
    init();
    bool res;
    int num=0;
    while(processing){
        process();
        ++num;
        if(num>4096)
            break;
    }
    if(result==NULL)
        failed = true;
    if(failed)
        res = false;
    else if(result){
        buildRoad(callback);
        res = true;
    }

    clear();

    return res;
}

pathFinding::pathFinding(){
    texture_pathTarget = driver->getTexture("../../res/icon/pathTarget.png");
    texture_pathPoint = driver->getTexture("../../res/icon/pathPoint.png");
    pathFindingMode = false;
}

bool pathFinding::findPath(const core::vector3df & A, const core::vector3df & B, std::list<core::vector3df> & r){
    blockPosition a(floor(A.X/2),floor(A.Y/2),floor(A.Z/2));
    blockPosition b(floor(B.X/2),floor(B.Y/2)+1,floor(B.Z/2));
    return findPath(a,b,[&](const building::blockPosition & p){
        r.push_front(core::vector3df(
                        p.x*2+1,
                        p.y*2+1,
                        p.z*2+1));
    });
}

void pathFinding::findPathByRay(const vec3 & start,const vec3 & end){
    if(mainControlBody==NULL)
        return;
    fetchByRay(start , end,[&](const vec3 & p,bodyInfo * ){
        std::list<core::vector3df> l;
        auto tg = scene->addBillboardSceneNode(0,core::dimension2d<f32>(5,5),p+vec3(0,1,0));
        tg->setMaterialTexture(0,texture_pathTarget);
        tg->setMaterialFlag(irr::video::EMF_LIGHTING, false );
        tg->setMaterialType(irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL);
        auto am = scene->createDeleteAnimator(1000);
        tg->addAnimator(am);
        if(findPath(mainControlBody->node->getPosition(),p,l)){
            for(auto it:l){
                auto pn = scene->addBillboardSceneNode(0,core::dimension2d<f32>(0.2,0.2),it);
                pn->setMaterialTexture(0,texture_pathPoint);
                pn->setMaterialFlag(irr::video::EMF_LIGHTING, false );
                pn->setMaterialType(irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL);
                pn->addAnimator(am);
            }
        }
        am->drop();
    });
}

void pathFinding::onDraw(){
    body::onDraw();
    if(pathFindingMode){
        irr::video::SColor col(64,255,255,255);
        driver->draw2DLine(irr::core::vector2d<irr::s32>(screenCenter.X-10,screenCenter.Y),irr::core::vector2d<irr::s32>(screenCenter.X+10,screenCenter.Y),col);
        driver->draw2DLine(irr::core::vector2d<irr::s32>(screenCenter.X,screenCenter.Y-10),irr::core::vector2d<irr::s32>(screenCenter.X,screenCenter.Y+10),col);
    }
}

void pathFinding::cancle(){
    body::cancle();
    pathFindingMode = false;
}

}
