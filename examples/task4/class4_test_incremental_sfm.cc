//// Created by caoqi on 2018/8/28.//#include "defines.h"#include "functions.h"#include "sfm/bundler_common.h"#include "sfm/bundler_features.h"#include "sfm/bundler_matching.h"#include "sfm/bundler_intrinsics.h"#include "sfm/bundler_init_pair_without_homography_check.h"#include "sfm/bundler_tracks.h"#include "sfm/bundler_incremental.h"#include "math/transform.h"#include "sfm/camera_pose.h"#include "core/scene.h"#include "util/timer.h"#include <util/file_system.h>#include <core/bundle_io.h>#include <core/camera.h>#include <fstream>#include <iostream>#include <stdlib.h>#include "assert.h"#include "json/json.h"#include "fstream"#include "sfm/geodesy.h"void write_cam_position(std::vector<math::Vec3f> positions){    Json::Value root;    Json::StreamWriterBuilder builder;    for(int i=0; i< positions.size(); ++i){        Json::Value position;        position["x"] = positions[i][0];        position["y"] = positions[i][1];        position["z"] = positions[i][2];        root[i] = position;    }    std::string json_file = Json::writeString(builder, root);    std::ofstream ofs ;    ofs.open("./campositions.json");    ofs << json_file;    ofs.close();}void read_pwm(sfm::bundler::PairwiseMatching &pairwise_matching, const char* filename) {    std::ifstream ifs;    ifs.open(filename);    assert(ifs.is_open());    Json::Value root;    Json::CharReaderBuilder builder;    builder["collectComments"] = false;    JSONCPP_STRING errs;    if (!parseFromStream(builder, ifs, &root, &errs)) //从ifs中读取数据到jsonRoot    {        return;    }    for (int i = 0; i < root.size(); ++i) {        Json::Value tvm = root[i];      //  int x = root[i]["view_1_id"].asInt();        sfm::bundler:: TwoViewMatching matching;        matching.view_1_id = tvm["view_1_id"].asInt();        matching.view_2_id = tvm["view_2_id"].asInt();        Json::Value matches = tvm["matches"];        for (int j = 0; j < matches.size(); j++) {            sfm::CorrespondenceIndex idx;            idx.first = matches[j]["first"].asInt();            idx.second = matches[j]["second"].asInt();            matching.matches.push_back(idx);        }        pairwise_matching.push_back(matching);    }}void read_viewports(sfm::bundler::ViewportList  &viewportList, char const* filename){    std::ifstream ifs;    ifs.open(filename);    assert(ifs.is_open());    Json::Value root;    Json::CharReaderBuilder builder;    builder["collectComments"] = false;    JSONCPP_STRING errs;    if (!parseFromStream(builder, ifs, &root, &errs)) //从ifs中读取数据到jsonRoot    {        return;    }    for (int i = 0; i < root.size(); ++i) {        sfm::bundler::Viewport vp;        Json::Value viewport = root[i];        vp.focal_length = viewport["forcllength"].asFloat();        vp.features.width = viewport["features"]["width"].asInt();        vp.features.height = viewport["features"]["height"].asInt();        for(int j =0; j< viewport["features"]["positions"].size(); ++j)        {            math::Vec2f point;            point[0] = viewport["features"]["positions"][j]["x"].asFloat();            point[1] = viewport["features"]["positions"][j]["y"].asFloat();            vp.features.positions.push_back(point);            math::Vec3uc color;            color[0] = viewport["features"]["colors"][j]["r"].asUInt();            color[1] = viewport["features"]["colors"][j]["g"].asUInt();            color[2] = viewport["features"]["colors"][j]["b"].asUInt();            vp.features.colors.push_back(color);        }        viewportList.push_back(vp);    }}/** * \description write gps message per line into tempLine * components templine with lon, lat, and ati * save them into std::vector< math::Vec3d > */std::vector< math::Vec3d > getGps(std::string posFile){    std::ifstream file1;    file1.open(posFile);    assert(file1.is_open());    std::string tempLine;    std::vector< math::Vec3d>  gpsInfo;    while(getline(file1, tempLine)){        std::cout << "Read from file " << tempLine << std::endl;        math::Vec3d tem3d;        std::string strTem;        std::stringstream input(tempLine);        input >> strTem;        input >> tem3d[0];        input >> tem3d[1];        input >> tem3d[2];        gpsInfo.push_back(tem3d);    }    return  gpsInfo;}/** *\description 创建一个场景 * @param image_folder_path * @param scene_path * @return */core::Scene::Ptrmake_scene(const std::string & image_folder_path, const std::string & scene_path){    util::WallTimer timer;    /*** 创建文件夹 ***/    const std::string views_path = util::fs::join_path(scene_path, "views/");    util::fs::mkdir(scene_path.c_str());    util::fs::mkdir(views_path.c_str());    /***扫描文件夹，获取所有的图像文件路径***/    util::fs::Directory dir;    try {dir.scan(image_folder_path);    }    catch (std::exception&e){        std::cerr << "Error scanning input dir: " << e.what() << std::endl;        std::exit(EXIT_FAILURE);    }    std::cout << "Found " << dir.size() << " directory entries." << std::endl;    core::Scene::Ptr scene= core::Scene::create("");    /**** 开始加载图像 ****/    std::sort(dir.begin(), dir.end());    int num_imported = 0;    for(std::size_t i=0; i< dir.size(); i++){        // 是一个文件夹        if(dir[i].is_dir){            std::cout<<"Skipping directory "<<dir[i].name<<std::endl;            continue;        }        std::string fname = dir[i].name;        std::string afname = dir[i].get_absolute_name();        // 从可交换信息文件中读取图像焦距        std::string exif;        core::ImageBase::Ptr image = load_any_image(afname, & exif);        if(image == nullptr){            continue;        }        core::View::Ptr view = core::View::create();        view->set_id(num_imported);        view->set_name(remove_file_extension(fname));        // 限制图像尺寸        int orig_width = image->width();        image = limit_image_size(image, MAX_PIXELS);        if (orig_width == image->width() && has_jpeg_extension(fname))            view->set_image_ref(afname, "original");        else            view->set_image(image, "original");        add_exif_to_view(view, exif);        scene->get_views().push_back(view);        /***保存视角信息到本地****/        std::string mve_fname = make_image_name(num_imported);        std::cout << "Importing image: " << fname                  << ", writing MVE view: " << mve_fname << "..." << std::endl;        view->save_view_as(util::fs::join_path(views_path, mve_fname));        num_imported+=1;    }    std::cout << "Imported " << num_imported << " input images, "              << "took " << timer.get_elapsed() << " ms." << std::endl;    return scene;}/** * * @param scene * @param viewports * @param pairwise_matching */voidfeatures_and_matching (core::Scene::Ptr scene,                       sfm::bundler::ViewportList* viewports,                       sfm::bundler::PairwiseMatching* pairwise_matching){    /* Feature computation for the scene. */    sfm::bundler::Features::Options feature_opts;    feature_opts.image_embedding = "original";    feature_opts.max_image_size = MAX_PIXELS;    feature_opts.feature_options.feature_types = sfm::FeatureSet::FEATURE_SIFT;    std::cout << "Computing image features..." << std::endl;    {        util::WallTimer timer;        sfm::bundler::Features bundler_features(feature_opts);        bundler_features.compute(scene, viewports);        std::cout << "Computing features took " << timer.get_elapsed()                  << " ms." << std::endl;        std::cout<<"Feature detection took " + util::string::get(timer.get_elapsed()) + "ms."<<std::endl;    }    /* Exhaustive matching between all pairs of views. */    sfm::bundler::Matching::Options matching_opts;    //matching_opts.ransac_opts.max_iterations = 1000;    //matching_opts.ransac_opts.threshold = 0.0015;    matching_opts.ransac_opts.verbose_output = false;    matching_opts.use_lowres_matching = false;    matching_opts.match_num_previous_frames = false;    matching_opts.matcher_type = sfm::bundler::Matching::MATCHER_EXHAUSTIVE;    std::cout << "Performing feature matching..." << std::endl;    {        util::WallTimer timer;        sfm::bundler::Matching bundler_matching(matching_opts);        bundler_matching.init(viewports);        bundler_matching.compute(pairwise_matching);        std::cout << "Matching took " << timer.get_elapsed()                  << " ms." << std::endl;        std::cout<< "Feature matching took "                          + util::string::get(timer.get_elapsed()) + "ms."<<std::endl;    }    if (pairwise_matching->empty())    {        std::cerr << "Error: No matching image pairs. Exiting." << std::endl;        std::exit(EXIT_FAILURE);    }}int main(int argc, char *argv[]){    if(argc < 3){        std::cout<<"Usage: [input]image_dir [output]scen_dir"<<std::endl;        return -1;    }    core::Scene::Ptr scene = make_scene(argv[1], argv[2]);    std::cout<<"Scene has "<<scene->get_views().size()<<" views. "<<std::endl;    sfm::bundler::ViewportList viewports;    sfm::bundler::PairwiseMatching pairwise_matching;    //get pwm and viewports from json files   // read_pwm(pairwise_matching,"./pwm.json");    //read_viewports(viewports, "./viewports.json");    features_and_matching(scene, &viewports, &pairwise_matching );    /* Drop descriptors and embeddings to save memory. */    scene->cache_cleanup();    for (std::size_t i = 0; i < viewports.size(); ++i)        viewports[i].features.clear_descriptors();    /* Check if there are some matching images. */    if (pairwise_matching.empty()) {        std::cerr << "No matching image pairs. Exiting." << std::endl;        std::exit(EXIT_FAILURE);    }    // 计算相机内参数，从Exif中读取    {        sfm::bundler::Intrinsics::Options intrinsics_opts;        std::cout << "Initializing camera intrinsics..." << std::endl;        sfm::bundler::Intrinsics intrinsics(intrinsics_opts);        intrinsics.compute(scene, &viewports);    }    /****** 开始增量的捆绑调整*****/    util::WallTimer timer;    /* Compute connected feature components, i.e. feature tracks. */    sfm::bundler::TrackList tracks;    {        sfm::bundler::Tracks::Options tracks_options;        tracks_options.verbose_output = true;        sfm::bundler::Tracks bundler_tracks(tracks_options);        std::cout << "Computing feature tracks..." << std::endl;        //计算track，此时只包含特征点信息，没有三维点坐标        bundler_tracks.write_pairwisematching(pairwise_matching);        bundler_tracks.write_viewportlist(&viewports);        bundler_tracks.compute(pairwise_matching, &viewports, &tracks);        std::cout << "Created a total of " << tracks.size()                  << " tracks." << std::endl;    }    /* Remove color data and pairwise matching to save memory. */    /*    for (std::size_t i = 0; i < viewports.size(); ++i)        viewports[i].features.colors.clear();        */    pairwise_matching.clear();    // 计算初始的匹配对    sfm::bundler::InitialPair::Result init_pair_result;    sfm::bundler::InitialPair::Options init_pair_opts;        //init_pair_opts.homography_opts.max_iterations = 1000;        //init_pair_opts.homography_opts.threshold = 0.005f;        init_pair_opts.homography_opts.verbose_output = false;        init_pair_opts.max_homography_inliers = 0.8f;        init_pair_opts.verbose_output = true;        // 开始计算初始的匹配对        sfm::bundler::InitialPair init_pair(init_pair_opts);        init_pair.initialize(viewports, tracks);        init_pair.compute_pair(&init_pair_result);    if (init_pair_result.view_1_id < 0 || init_pair_result.view_2_id < 0        || init_pair_result.view_1_id >= static_cast<int>(viewports.size())        || init_pair_result.view_2_id >= static_cast<int>(viewports.size()))    {        std::cerr << "Error finding initial pair, exiting!" << std::endl;        std::cerr << "Try manually specifying an initial pair." << std::endl;        std::exit(EXIT_FAILURE);    }    std::cout << "Using views " << init_pair_result.view_1_id              << " and " << init_pair_result.view_2_id              << " as initial pair." << std::endl;    /* Incrementally compute full bundle. */    sfm::bundler::Incremental::Options incremental_opts;    incremental_opts.pose_p3p_opts.max_iterations = 1000;    incremental_opts.pose_p3p_opts.threshold = 0.005f;    incremental_opts.pose_p3p_opts.verbose_output = false;    incremental_opts.track_error_threshold_factor = TRACK_ERROR_THRES_FACTOR;    incremental_opts.new_track_error_threshold = NEW_TRACK_ERROR_THRES;    incremental_opts.min_triangulation_angle = MATH_DEG2RAD(1.0);    incremental_opts.ba_fixed_intrinsics = false;    //incremental_opts.ba_shared_intrinsics = conf.shared_intrinsics;    incremental_opts.verbose_output = true;    incremental_opts.verbose_ba = true;    /* Initialize viewports with initial pair. */    viewports[init_pair_result.view_1_id].pose = init_pair_result.view_1_pose;    viewports[init_pair_result.view_2_id].pose = init_pair_result.view_2_pose;    /* Initialize the incremental bundler and reconstruct first tracks. */    sfm::bundler::Incremental incremental(incremental_opts);    incremental.initialize(&viewports, &tracks);    // 对当前两个视角进行track重建，并且如果track存在外点，则将每个track的外点剥离成新的track    incremental.triangulate_new_tracks(2);    // 根据重投影误差进行筛选    incremental.invalidate_large_error_tracks();    /* Run bundle adjustment. */    std::cout << "Running full bundle adjustment..." << std::endl;    incremental.bundle_adjustment_full();    /* Reconstruct remaining views. */    int num_cameras_reconstructed = 2;    int full_ba_num_skipped = 0;    while (true)    {        /* Find suitable next views for reconstruction. */        std::vector<int> next_views;        incremental.find_next_views(&next_views);        /* Reconstruct the next view. */        int next_view_id = -1;        for (std::size_t i = 0; i < next_views.size(); ++i)        {            std::cout << std::endl;            std::cout << "Adding next view ID " << next_views[i]                      << " (" << (num_cameras_reconstructed + 1) << " of "                      << viewports.size() << ")..." << std::endl;            //重建新的视角， 用RANSAC-P3P恢复相机姿态            if (incremental.reconstruct_next_view(next_views[i]))            {                next_view_id = next_views[i];                break;            }        }        if (next_view_id < 0) {            if (full_ba_num_skipped == 0) {                std::cout << "No valid next view." << std::endl;                std::cout << "SfM reconstruction finished." << std::endl;                std::vector< math::Vec3f > camPosition;                //math::Matrix<double, 3, 4> Pose;                for(size_t i = 0; i< viewports.size(); ++i)                {                    sfm::CameraPose temP = viewports[i].pose;                    math::Vec3f tempCam = -temP.R.transpose() * temP.t;                    camPosition.push_back(tempCam);                }                write_cam_position(camPosition);                break;            }            else            {                incremental.triangulate_new_tracks(MIN_VIEWS_PER_TRACK);                std::cout << "Running full bundle adjustment..." << std::endl;                incremental.bundle_adjustment_full();                incremental.invalidate_large_error_tracks();                full_ba_num_skipped = 0;                continue;            }        }        //重建相机姿态成功了，此时的相机姿态是通过pnp方法求得的，需要进行一次捆绑调整        /* Run single-camera bundle adjustment. */        std::cout << "Running single camera bundle adjustment..." << std::endl;        incremental.bundle_adjustment_single_cam(next_view_id);        num_cameras_reconstructed += 1;        /* Run full bundle adjustment only after a couple of views. */        int const full_ba_skip_views =  std::min(100, num_cameras_reconstructed / 10);        if (full_ba_num_skipped < full_ba_skip_views)        {            std::cout << "Skipping full bundle adjustment (skipping "                      << full_ba_skip_views << " views)." << std::endl;            full_ba_num_skipped += 1;        }        else        {            //对重建的新的视角进行三角测量获得三维点            incremental.triangulate_new_tracks(MIN_VIEWS_PER_TRACK);            std::cout << "Running full bundle adjustment..." << std::endl;            //全局的捆绑调整            incremental.bundle_adjustment_full();            incremental.invalidate_large_error_tracks(); //Tracks滤波可以放在前面            full_ba_num_skipped = 0;        }    }    sfm::bundler::TrackList valid_tracks;    for(int i=0; i<tracks.size(); i++){        if(tracks[i].is_valid()){            valid_tracks.push_back(tracks[i]);        }    }    std::cout << "SfM reconstruction took " << timer.get_elapsed()              << " ms." << std::endl;    std::cout<< "SfM reconstruction took "                      + util::string::get(timer.get_elapsed()) + "ms."<<std::endl;    /***** 保存输出结果***/    std::ofstream out_file("./points.ply");    assert(out_file.is_open());    out_file<<"ply"<<std::endl;    out_file<<"format ascii 1.0"<<std::endl;    out_file<<"element vertex "<<valid_tracks.size()<<std::endl;    out_file<<"property float x"<<std::endl;    out_file<<"property float y"<<std::endl;    out_file<<"property float z"<<std::endl;    out_file<<"property uchar red"<<std::endl;    out_file<<"property uchar green"<<std::endl;    out_file<<"property uchar blue"<<std::endl;    out_file<<"end_header"<<std::endl;    for(int i=0; i< valid_tracks.size(); i++){        out_file<<valid_tracks[i].pos[0]<<" "<< valid_tracks[i].pos[1]<<" "<<valid_tracks[i].pos[2]<<" "                <<(int)valid_tracks[i].color[0]<<" "<<(int)valid_tracks[i].color[1]<<" "<<(int)valid_tracks[i].color[2]<<std::endl;    }    out_file.close();    /* Normalize scene if requested. *///    if (conf.normalize_scene)//    {//        std::cout << "Normalizing scene..." << std::endl;//        incremental.normalize_scene();//    }    /* Save bundle file to scene. */    std::cout << "Creating bundle data structure..." << std::endl;    core::Bundle::Ptr bundle = incremental.create_bundle();    core::save_mve_bundle(bundle, std::string(argv[2]) + "/synth_0.out");    /* Apply bundle cameras to views. */    core::Bundle::Cameras const& bundle_cams = bundle->get_cameras();    core::Scene::ViewList const& views = scene->get_views();    if (bundle_cams.size() != views.size())    {        std::cerr << "Error: Invalid number of cameras!" << std::endl;        std::exit(EXIT_FAILURE);    }#pragma omp parallel for schedule(dynamic,1)    for (std::size_t i = 0; i < bundle_cams.size(); ++i)    {        core::View::Ptr view = views[i];        core::CameraInfo const& cam = bundle_cams[i];        if (view == nullptr)            continue;        if (view->get_camera().flen == 0.0f && cam.flen == 0.0f)            continue;        view->set_camera(cam);        /* Undistort image. */        if (!undistorted_name.empty())        {            core::ByteImage::Ptr original                    = view->get_byte_image(original_name);            if (original == nullptr)                continue;            core::ByteImage::Ptr undist                    = core::image::image_undistort_k2k4<uint8_t>                            (original, cam.flen, cam.dist[0], cam.dist[1]);            view->set_image(undist, undistorted_name);        }#pragma omp critical        std::cout << "Saving view " << view->get_directory() << std::endl;        view->save_view();        view->cache_cleanup();    }   // log_message(conf, "SfM reconstruction done.\n");    /* 严峻考验，对齐点云     * 先读取GPS信息，然后GPS转UTM坐标     * 然后用shiji umeyama方法估计s,r,t*/    //读取GPS信息    //pos文件地址    std::string posFile = "/home/cao/图片/pix4dtestdata/1.txt";    //获取    std::vector< math::Vec3d > gpsInfo = getGps(posFile);    //BL 转 UTM    //std::vector< math::Vec3d> ecefData;    std::vector< math::Vec3d> utmData;    std::vector< sfm::Vec3> vec_sfm_center, vec_gps_center;    for(size_t i=0; i<gpsInfo.size(); ++i)    {       //math::Vec3d ecef = sfm::geodesy::lla_to_ecef(gpsInfo[i][0], gpsInfo[i][1], gpsInfo[i][2]);       math::Vec3d utm = sfm::geodesy::lla_to_utm(gpsInfo[i][0], gpsInfo[i][1],gpsInfo[i][2]);       sfm::Vec3 _utm(utm[0],utm[1],utm[2]);       utmData.push_back(utm);       vec_gps_center.push_back(_utm);    }    //获取相机位置    std::vector< math::Vec3d > poses ;    for(size_t i=0; i< bundle_cams.size(); ++i)    {        core::CameraInfo const& cam = bundle_cams[i];        float pose[3];        cam.fill_camera_pos(pose);        poses.push_back(math::Vec3d(pose[0],pose[1],pose[2]));        vec_sfm_center.push_back(sfm::Vec3(pose[0],pose[1],pose[2]));    }    //用 svd分解方法 估计 s r t    /* Determine transformation. */    math::Matrix3d R;    double s;    math::Vec3d t;    if (!math::determine_transform(poses, utmData, &R, &s, &t))        return -1;    std::cout << R << "\n";    std::cout << s << "\n";    std::cout << t << "\n";    //利用S，R，T变换原有的点云    //变换后点云    /***** 保存输出结果***/    std::ofstream out_file1("./absolute_points.ply");    assert(out_file1.is_open());    out_file1<<"ply"<<std::endl;    out_file1<<"format ascii 1.0"<<std::endl;    out_file1<<"element vertex "<<valid_tracks.size()<<std::endl;    out_file1<<"property float x"<<std::endl;    out_file1<<"property float y"<<std::endl;    out_file1<<"property float z"<<std::endl;    out_file1<<"property uchar red"<<std::endl;    out_file1<<"property uchar green"<<std::endl;    out_file1<<"property uchar blue"<<std::endl;    out_file1<<"end_header"<<std::endl;    //std::vector<math::Vec3f> absolute_points;    for(size_t i =0; i<valid_tracks.size(); ++i)    {        math::Vec3f tem = R*s*valid_tracks[i].pos+t;        out_file1<<tem[0]<<" "<< tem[1]<<" "<<tem[2]<<" "                <<(int)valid_tracks[i].color[0]<<" "<<(int)valid_tracks[i].color[1]<<" "<<(int)valid_tracks[i].color[2]<<std::endl;    }    out_file1.close();    //用shinji umeyama 方法估计_S，_R，_T    // Convert positions to the appropriate data container    const sfm::Mat X_SFM = Eigen::Map<sfm::Mat >(vec_sfm_center[0].data(), 3, vec_sfm_center.size());    const sfm::Mat X_GPS = Eigen::Map<sfm::Mat>(vec_gps_center[0].data(), 3, vec_gps_center.size());    sfm::Vec3 _t;    sfm::Mat3 _R;    double _S;    if(!sfm::CameraPose::FindRTS(X_SFM,X_GPS,&_S,&_t,&_R))    {        std::cerr << "Failed to comute the registration" << std::endl;        return EXIT_FAILURE;    }    std::cout<<_S<<"\n";    std::cout<<_R<<"\n";    std::cout<<_t<<"\n";    return 0;}