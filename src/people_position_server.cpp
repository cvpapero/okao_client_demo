/*
2015.3.9---------------
今後実装すること
magniに基づいて、OKAO_IDを信頼してそのまま保存するか
あるいは、不安定だけど一応OKAO_ID暫定的に保存するか


d_idに基づいてパブリッシュ

セーブポイント対応
ファイル出力機能

2015.1.9----------------

今は、ruleに入ってるキーワードを判定した条件分岐で、検索の処理をしている
つまり、キーワードの数だけ条件分岐がいる

 
*/

#include <ros/ros.h>
#include <geometry_msgs/PointStamped.h>
#include <tf/transform_listener.h>

#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
//#include <time.h>

//オリジナルメッセージ
#include <humans_msgs/PersonPoseImgArray.h>
#include "humans_msgs/Humans.h"
#include "humans_msgs/HumanSrv.h"
#include "humans_msgs/HumansSrv.h"
#include "okao_client/OkaoStack.h"
//#include "okao_client/OkaoStack.h"
#include "MsgToMsg.hpp"

#define MAGNI 1.5

using namespace std;

//d_idに基づくDB
//map<int, humans_msgs::Human> d_DBHuman;
map<long long, humans_msgs::Human> d_DBHuman;
//map<int, humans_msgs::Human> p_DBHuman;

//okao_idに基づくDB
map<int, humans_msgs::Human> o_DBHuman;

class PeoplePositionServer
{
private:
  ros::NodeHandle n;
  ros::Subscriber rein_sub_;
  ros::Publisher face_now_pub_ ;
  ros::ServiceServer track_srv_;
  ros::ServiceServer okao_srv_;
  ros::ServiceServer name_srv_;
  ros::ServiceClient okaoStack_;
  ros::ServiceServer array_srv_;

  boost::thread* pub_thread;

  //humans_msgs::Human human


  //map[1] = 

  //map<int, humans_msgs::Human> o_DBHuman;
  //人物の初期化をする
  //humans_msgs::Human init;
  //o_DBHuman[1] = 


public:
  PeoplePositionServer()
  {
    rein_sub_ 
      = n.subscribe("/humans/recog_info", 1, 
		    &PeoplePositionServer::callback, this);
    face_now_pub_
      = n.advertise<humans_msgs::PersonPoseImgArray
		    >("/now_human", 10);

    track_srv_ 
      = n.advertiseService("track_srv", 
			  &PeoplePositionServer::resTrackingId, this);
    okao_srv_ 
      = n.advertiseService("okao_srv", 
			   &PeoplePositionServer::resOkaoId, this);

     name_srv_ 
       = n.advertiseService("name_srv", 
			    &PeoplePositionServer::resName, this);
     
    okaoStack_
      = n.serviceClient<okao_client::OkaoStack>("stack_send");

    pub_thread = new boost::thread(boost::bind(&PeoplePositionServer::allHumanPublisher, this));

    //file_input();
    //allHumanPublisher();
  }
  
  ~PeoplePositionServer()
  {

    //file_output();
    d_DBHuman.clear();
    o_DBHuman.clear();
  }

  //o_DBHumanに書き込みのみ
  //mat画像はokao_stack2がやる
  void file_input()
  {
    cout << "file open" << endl;
    //まず、ファイルは以下のような形式にする
    //okao_id,name,hist,x,y,z 
    //okao_idをみて、人物の位置をo_DBHumanに詰め込んでいく   
    std::ifstream ifs("/home/roomba/peopledata.txt");
    if(ifs.fail())
      {  // if(!fin)でもよい。
	cout << "file not open" << endl;
	return;
      }
    humans_msgs::Human fhum;
    humans_msgs::Person fperson;
    while( (ifs >> fhum.max_okao_id >> fperson.name >> fhum.max_hist >> fhum.p.x >> fhum.p.y >> fhum.p.z )!=0 )
      {
	fhum.face.persons.push_back( fperson );
	fhum.header.frame_id = "map";
	fhum.header.stamp = ros::Time::now();
	o_DBHuman[ fhum.max_okao_id ] = fhum;
	cout <<"input:"<<endl << fhum.max_okao_id << endl << fhum.p << endl;
      }
    //cout << 
  }
 
  //ファイル出力する
  //o_DBHuman
  void file_output()
  {
    cout << "file write" << endl;
    stringstream ss;
    map<int, humans_msgs::Human>::iterator it_o = o_DBHuman.begin();
    while( it_o != o_DBHuman.end() )
      {	
	ss << it_o->second.max_okao_id << " " << "test" //it_o->second.face.persons[0].name 
	   << " " << it_o->second.max_hist << " " << it_o->second.p.x 
	   << " " << it_o->second.p.y << " " << it_o->second.p.z <<endl; 
	++it_o; 
      }

    std::ofstream ofs("/home/roomba/peopledata.txt");

    if(ofs.fail())
      {  // if(!fout)でもよい。
        cout << "file not open" << endl;
        return;
      }
    ofs << ss.str() <<endl;
    cout << "write data:" << ss.str() <<endl;
  }

  void callback(const humans_msgs::HumansConstPtr& rein)
  {

    if( rein->num == 0 )
      {
	//ここで、見えなくなった人物のd_idをパブリッシュしてもいいかも
	//もし、N_DBHumanの中にデータが入っていたら、d_idをpeople_recog_infoにreq/resして、.clear()する
	//n_DBHuman.size();
      }
    else
      {

	for(int i = 0; i < rein->num; ++i)
	  {
	    humans_msgs::Human ah;
	    ah.header.frame_id = "map";//重要
	    ah.header.stamp = rein->header.stamp;
	    transformPosition( rein->human[ i ] ,&ah );
	    //ah.magni = rein->human[ i ].magni;
	    //ah.header.stamp = ros::Time::now();
	    //ah = rein->human[ i ];
	    //n_DBHuman[ i ] = ah;
	    d_DBHuman[ rein->human[ i ].body.tracking_id ] = ah;
	    o_DBHuman[ rein->human[ i ].max_okao_id ] = ah;

	    ROS_INFO("people data update! okao_id: %d", rein->human[ i ].max_okao_id );
	    //cout << "people data update! okao_id: " <<endl;;
	  } 
      } 
    //allHumanPublisher();
  }

  
  void debugAllHuman()
  {
   
    ROS_INFO("debug all human");
  
    map<int, humans_msgs::Human>::iterator it_o = o_DBHuman.begin();
    while( it_o != o_DBHuman.end() )
      {
	humans_msgs::Human h_res;	
	//cout <<"name: "<< it_o->second.max_okao_id << " d_id:" << it_o->second.d_id<< endl; 
	++it_o; 
      }

  }


  void allHumanPublisher()
  {
    ros::NodeHandle nmv;
    ros::Rate rate(5);
    //ROS_INFO("all human publisher");
    while(nmv.ok())
      {
	humans_msgs::PersonPoseImgArray ppia;
	//o_DBHuman内から、tracking_idをキーにして検索
	map<long long, humans_msgs::Human>::iterator it_d = d_DBHuman.begin();
	
	vector<int> okao_id_log;
	//map<int, humans_msgs::Human>::iterator it_d = o_DBHuman.begin();
	ros::Time now = ros::Time::now();
	while( it_d != d_DBHuman.end() )
	  {
	    //	cout <<"name: "<< it_o->second.max_okao_id << " d_id:" << it_o->second.d_id<< endl; 
	    if( checkHist(it_d->second) )
	      {
	       
		if(checkTime(it_d->second, now))
		  {
		    cout << "now checkOkaoId ok"<<endl;
		    humans_msgs::PersonPoseImg ppi;
		    getOkaoStack( it_d->second, &ppi );	
		    ppia.ppis.push_back( ppi );
		    okao_id_log.push_back( it_d->second.max_okao_id );
		  }
		else
		  {
		    d_DBHuman.erase(it_d++);
		  }
	      }

	    ++it_d; 
	  }
	ppia.header.stamp = ros::Time::now();
	ppia.header.frame_id = "map";
	face_now_pub_.publish( ppia );

	rate.sleep();
      }
  }
  
  bool checkHist(humans_msgs::Human hum)
  {
    if(hum.max_hist>30)
      return true;
    else
      return false;
  }

  bool checkTime(humans_msgs::Human hum, ros::Time now)
  {
    ros::Duration time_diff = now - hum.header.stamp;
    if(time_diff > ros::Duration(5))
      return false;
    else
      return true;
  }

  bool checkOkaoId(int okao_id, vector<int> okao_id_log)
  {
    cout << "check id:" << okao_id << endl; 
    vector<int>::iterator itr = find(okao_id_log.begin(), okao_id_log.end(), okao_id);
    if( itr!=okao_id_log.end() )
      {
	cout << "exist" << endl;
	return false;
      }
    else
      {
	cout << "not exist" << endl;
	return true;
      }
  }

  void getOkaoStack(humans_msgs::Human hum, humans_msgs::PersonPoseImg *ppi)
  {
    okao_client::OkaoStack stack;
    stack.request.header.stamp = hum.header.stamp;

    //cout << "magni: "<< (double)hum.magni<<endl; 

    //if((double)hum.magni > (double)MAGNI)
    //  {
    stack.request.person.okao_id = hum.max_okao_id;
    //  }
    /*
    else
      {
	stack.request.person.okao_id = 0;
      }
    */
    if( okaoStack_.call( stack ) )
      {
    
	ppi->person.hist = hum.max_hist;
	ppi->person.okao_id = hum.max_okao_id;
	ppi->person.name = stack.response.person.name;
	ppi->pose.position = hum.p;
	ppi->pose.orientation.w = 1;
	
	ppi->image = stack.response.image;
	ppi->header.stamp = ros::Time::now();
	ppi->header.frame_id = hum.header.frame_id;
	//cout << hum.header.frame_id <<endl;
	//dbhuman[rein->human[i].max_okao_id] = ppi;
      }
    else
      {
	cout << "okao_id: " << hum.max_okao_id << " has not stack!" <<endl;
      }
  }

  //現在はbody.pのみ
  void transformPosition(humans_msgs::Human hsrc, humans_msgs::Human *hdst)
  {
    //cout << "now looking: " << it_o->second.max_okao_id << endl;
    ros::Time t = ros::Time::now();
    geometry_msgs::PointStamped src;
    src.point = hsrc.p; 
    src.header.stamp = t;
    src.header.frame_id = hsrc.header.frame_id;
    geometry_msgs::PointStamped dst;
    dst.header.stamp = t;
    dst.header.frame_id = hdst->header.frame_id;

    MsgToMsg::transformHead(src, &dst);
    hdst->p = dst.point;
    hdst->header.stamp = t;
    hdst->header.frame_id = dst.header.frame_id;
    hdst->state = hsrc.state;
    hdst->max_okao_id = hsrc.max_okao_id;
    hdst->max_hist = hsrc.max_hist;

    //tracking_id
    hdst->body.tracking_id = hsrc.body.tracking_id;
  }


  void getPerson(humans_msgs::Human hsrc, humans_msgs::Face *hdst)
  {
    okao_client::OkaoStack stack;
    stack.request.person.okao_id = hsrc.max_okao_id;
    okaoStack_.call( stack );

    humans_msgs::Person person;
    person = stack.response.person;
    hdst->persons.push_back( person );
  }
  

  bool resTrackingId(humans_msgs::HumanSrv::Request &req,
		     humans_msgs::HumanSrv::Response &res)
  {
    cout<<"tracking_id:"<< req.src.body.tracking_id << endl;

    //o_DBHuman内から、tracking_idをキーにして検索
    map<int, humans_msgs::Human>::iterator it_o = o_DBHuman.begin();
    while( it_o != o_DBHuman.end() )
      {
	if( it_o->second.body.tracking_id == req.src.body.tracking_id )
	  {
	    humans_msgs::Human h_res;
	    humans_msgs::Body b_res;
	    humans_msgs::Face f_res;
	    h_res.header.frame_id = req.src.header.frame_id;
	    getPerson(it_o->second, &f_res);
	    h_res.max_okao_id = it_o->second.max_okao_id;
	    h_res.max_hist = it_o->second.max_hist;
	    h_res.header = it_o->second.header;
	    
	    h_res.p = it_o->second.p;
	    h_res.face = f_res;
	    res.dst = h_res;
	    cout <<"response name: " << f_res.persons[0].name << endl;
	    return true;
	  }
	++it_o;
      }
    cout<<"no tracking_id:"<< req.src.body.tracking_id << endl;
    return false;
  }
  
  bool resOkaoId(humans_msgs::HumanSrv::Request &req,
		 humans_msgs::HumanSrv::Response &res)
    
  {
    //n_DBHumanについての検索
    //もしそれでもみつからなかったらp_DBHumanから検索する
    //どのデータベースから見つかったかをラベルづけして返す(.srvに記述しておく)
    cout<<"okao_id: "<< req.src.max_okao_id << endl;

    map<int, humans_msgs::Human>::iterator it_o = o_DBHuman.begin();
    while( it_o != o_DBHuman.end() )
      {
	if( it_o->second.max_okao_id == req.src.max_okao_id )
	  {
	    humans_msgs::Human h_res;
	    humans_msgs::Body b_res;
	    humans_msgs::Face f_res;
	    h_res.header.frame_id = req.src.header.frame_id;
	    getPerson(it_o->second, &f_res);
	    h_res.max_okao_id = it_o->second.max_okao_id;
	    h_res.max_hist = it_o->second.max_hist;
	    h_res.header = it_o->second.header;
	    
	    h_res.p = it_o->second.p;
	    h_res.face = f_res;
	    res.dst = h_res;
	    return true;
	  }
	++it_o;
      }
    cout<<"no okao_id: "<< req.src.max_okao_id << endl;
    return false;
  }
  
  bool resName(humans_msgs::HumanSrv::Request &req,
	       humans_msgs::HumanSrv::Response &res)
    
  {
    //n_DBHumanについての検索
    //もしそれでもみつからなかったらp_DBHumanから検索する
    //どのデータベースから見つかったかをラベルづけして返す(.srvに記述しておく)
    cout<<"name: "<< req.src.face.persons[0].name << endl;
    map<int, humans_msgs::Human>::iterator it_o = o_DBHuman.begin();
    while( it_o != o_DBHuman.end() )
      {
	if( it_o->second.face.persons[0].name == req.src.face.persons[0].name )
	  {
	    humans_msgs::Human h_res;
	    humans_msgs::Body b_res;
	    humans_msgs::Face f_res;
	    h_res.header.frame_id = req.src.header.frame_id;
	    getPerson(it_o->second, &f_res);
	    h_res.max_okao_id = it_o->second.max_okao_id;
	    h_res.max_hist = it_o->second.max_hist;
	    h_res.header = it_o->second.header;
	    
	    h_res.p = it_o->second.p;
	    h_res.face = f_res;
	    res.dst = h_res;
	    return true;

	  }
	++it_o;
      }
    cout<<"no name: "<< req.src.face.persons[0].name << endl;
    return false;
  }
  
  /*
  bool resHumans(humans_msgs::HumansSrv::Request &req,
		 humans_msgs::HumansSrv::Response &res)
  {
    ROS_INFO("all people request!");
    humans_msgs::Humans hums;
    map<int, humans_msgs::Human>::iterator it = o_DBHuman.begin();
    int num = 0;
    while( it != o_DBHuman.end() )
      { 
	//ここで、mapが持っている値を次の値につめて返す
	humans_msgs::Human hum;
	hum = it->second;  
	hums.human.push_back( hum );
	++num;
	++it;
      }
    //hums.header
    hums.num = num;
    res.dst = hums;
    return true;
  }
  */
 
};

//thread  
int main(int argc, char** argv)
{
  ros::init(argc, argv, "people_position_server");
  PeoplePositionServer PPS;
  /*
  while(ros::ok())
    {
      PPS.allHumanPublisher();
      ros::spinOnce();
    }
  */
  ros::spin();
  return 0;
}
