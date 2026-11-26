// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/maintenance.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
void put(const QString& path,const QByteArray& data,bool old=true){QFile f(path);check(f.open(QIODevice::WriteOnly),"fixture file");f.write(data);f.close();if(old){timespec times[2]={{::time(nullptr)-60*86400,0},{::time(nullptr)-60*86400,0}};check(!::utimensat(AT_FDCWD,QFile::encodeName(path).constData(),times,AT_SYMLINK_NOFOLLOW),"fixture age");}}
bool fails(const std::function<void()>& fn){try{fn();return false;}catch(const std::exception&){return true;}}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);using namespace syspulse::maintenance;
try{
 QTemporaryDir temp;check(temp.isValid(),"private fixture root");const auto root=temp.path()+"/cache";QDir().mkpath(root+"/thumbnails/normal");
 const auto a=root+"/thumbnails/normal/"+QString(32,'a')+".png";const auto b=root+"/thumbnails/normal/"+QString(32,'b')+".png";
 put(a,"first");put(b,"second",false);put(root+"/thumbnails/normal/personal.txt","never touch");
 auto plan=scan(root,{"normal"},30);check(plan.files.size()==1&&plan.bytes==5,"scan only aged hash-named thumbnail files");check(QFile::exists(a),"scan is read only");
 auto staged=stage(plan);check(staged.changed==1&&!QFile::exists(a)&&QFile::exists(b),"only reviewed old thumbnail moved");auto rows=recoveries(root);check(rows.size()==1&&rows[0].files==1&&rows[0].bytes==5,"actual recovery contents measured");
 put(a,"regenerated",false);auto conflict=restore(root,staged.run);check(conflict.changed==0&&conflict.skipped==1,"restore never overwrites regenerated data");QFile::remove(a);
 auto restored=restore(root,staged.run);check(restored.changed==1&&QFile::exists(a)&&recoveries(root).isEmpty(),"restore recovers the original and removes empty run metadata");
 plan=scan(root,{"normal"},30);put(a,"changed since review");check(stage(plan).changed==0&&QFile::exists(a),"stale file identity cannot be staged");
 plan=scan(root,{"normal"},30);plan.files[0].directory="../outside";check(fails([&]{stage(plan);}),"forged path rejected before any move");
 auto good=scan(root,{"normal"},30);auto run=stage(good);check(run.changed==1,"second valid staging");auto removed=purge(root,run.run);check(removed.changed==1&&recoveries(root).isEmpty(),"explicit permanent removal only affects a recovery run");
 check(QFile::exists(root+"/thumbnails/normal/personal.txt"),"unrelated files preserved through purge");
 const auto outside=temp.path()+"/outside";put(outside,"outside must survive",false);::symlink(QFile::encodeName(outside).constData(),QFile::encodeName(a).constData());
 check(scan(root,{"normal"},1).files.isEmpty(),"leaf symlinks are never planned");QFile::remove(a);put(a,"held original");
 auto racePlan=scan(root,{"normal"},1);const auto old=a+".held";
 auto raced=stage(racePlan,[&]{QFile::rename(a,old);::symlink(QFile::encodeName(outside).constData(),QFile::encodeName(a).constData());});
 check(raced.changed==0&&QFile::exists(outside)&&QFile::exists(old),"atomic move identity check preserves a raced source and outside target");QFile::remove(a);QFile::remove(old);
 put(a,"linked");check(!::link(QFile::encodeName(a).constData(),QFile::encodeName(temp.path()+"/other-link").constData()),"hardlink fixture");check(scan(root,{"normal"},1).files.isEmpty(),"multiply linked files are skipped");
 check(fails([&]{scan(root,{"normal","normal"},30);}),"duplicate category rejected");check(fails([&]{scan(root,{"normal"},0);}),"age must be deliberate and bounded");
 const auto alias=temp.path()+"/cache-link";QFile::link(root,alias);check(fails([&]{scan(alias,{"normal"},30);}),"cache root symlink rejected");
 check(fails([&]{purge(root,"../outside");}),"invalid recovery ID cannot escape storage");
 const auto crashRoot=temp.path()+"/interrupted-cache";QDir().mkpath(crashRoot+"/thumbnails/normal");const auto first=crashRoot+"/thumbnails/normal/"+QString(32,'c')+".png";const auto second=crashRoot+"/thumbnails/normal/"+QString(32,'d')+".png";put(first,"first interrupted");put(second,"second interrupted");
 auto crashPlan=scan(crashRoot,{"normal"},30);int moved=0;check(fails([&]{stage(crashPlan,[&]{if(++moved==2)throw std::runtime_error("simulated interrupted worker");});}),"interrupted staging retains its prewritten manifest");const auto pending=recoveries(crashRoot);check(pending.size()==1&&pending[0].files==1,"partial staging is discoverable after an interrupted worker");check(restore(crashRoot,pending[0].id).changed==1&&QFile::exists(first)&&QFile::exists(second),"partial staging restores moved data and preserves unmoved data");
 auto expired=scan(crashRoot,{"normal"},30);expired.created-=3601;check(fails([&]{stage(expired);})&&QFile::exists(first)&&QFile::exists(second),"expired plans require a new scan without moving files");
 std::cout<<checks<<" cache planning, write-ahead recovery, conflict, race, symlink and hardlink checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
