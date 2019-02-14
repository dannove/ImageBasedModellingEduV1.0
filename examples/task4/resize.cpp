//
// Created by cao on 18-11-20.
//

#include <iostream>
#include <core/bundle_io.h>
#include "defines.h"

/**
 *\description 创建一个场景
 * @param image_folder_path
 * @param scene_path
 * @return
 */
core::Scene::Ptr
make_scene(const std::string & image_folder_path, const std::string & scene_path){

    util::WallTimer timer;

    /*** 创建文件夹 ***/
    const std::string views_path = util::fs::join_path(scene_path, "views/");
    util::fs::mkdir(scene_path.c_str());
    util::fs::mkdir(views_path.c_str());

    /***扫描文件夹，获取所有的图像文件路径***/
    util::fs::Directory dir;

    try {dir.scan(image_folder_path);
    }
    catch (std::exception&e){
        std::cerr << "Error scanning input dir: " << e.what() << std::endl;

        std::exit(EXIT_FAILURE);
    }
    std::cout << "Found " << dir.size() << " directory entries." << std::endl;

    core::Scene::Ptr scene= core::Scene::create("");

    /**** 开始加载图像 ****/
    std::sort(dir.begin(), dir.end());
    int num_imported = 0;
    for(std::size_t i=0; i< dir.size(); i++){
        // 是一个文件夹
        if(dir[i].is_dir){
            std::cout<<"Skipping directory "<<dir[i].name<<std::endl;
            continue;
        }

        std::string fname = dir[i].name;
        std::string afname = dir[i].get_absolute_name();

        // 从可交换信息文件中读取图像焦距
        std::string exif;
        core::ImageBase::Ptr image = load_any_image(afname, & exif);
        if(image == nullptr){
            continue;
        }

        core::View::Ptr view = core::View::create();
        view->set_id(num_imported);
        view->set_name(remove_file_extension(fname));

        // 限制图像尺寸
        int orig_width = image->width();
        image = limit_image_size(image, MAX_PIXELS);
        if (orig_width == image->width() && has_jpeg_extension(fname))
            view->set_image_ref(afname, "original");
        else
            view->set_image(image, "original");

        add_exif_to_view(view, exif);

        scene->get_views().push_back(view);

        /***保存视角信息到本地****/
        std::string mve_fname = make_image_name(num_imported);
        std::cout << "Importing image: " << fname
                  << ", writing MVE view: " << mve_fname << "..." << std::endl;
        view->save_view_as(util::fs::join_path(views_path, mve_fname));

        num_imported+=1;
    }

    std::cout << "Imported " << num_imported << " input images, "
              << "took " << timer.get_elapsed() << " ms." << std::endl;

    return scene;
}

int main(int argc, char *argv[]) {

    if (argc < 3) {
        std::cout << "Usage: [input]image_dir [output]scen_dir" << std::endl;
        return -1;
    }
}