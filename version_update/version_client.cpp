#include "version_client.h"
#include "../ca/ca.h"
#include "../ca/ca_message.h"
#include "../ca/ca_global.h"
#include "../ca/ca_serialize.h"
#include <random>
#include <chrono>
#include <sstream>
#include "HttpRequest.h"
#include "../common/config.h"
#include "../utils/util.h"

typedef struct net_pack
{
	int			len				=0;
	int			type			=0;
	int			data_len		=0;
	std::string	data			="";
	std::string	version			="";
}net_pack;



void Version_Client::start()
{
    m_update_thread = new thread(Version_Client::update, this);
    m_update_thread->detach();
    return;
}



void Version_Client::update(Version_Client * vc)
{   
    while (true)
    {   
        
        vc->init();
        
        if (vc->get_flag() == 0)
        {   
            
            
            vc->prepare_data_ca();
            vc->prepare_data_net();
            
            vc->send_verify_version_request();
        }

        if (vc->get_flag() == 1)
        {
            
            
            int res = vc->compare_version();
            if (res == 0)
            {
                cout << "版本更新成功！" << endl;
                
                node_info ver_node;
                vc->get_version_server_info(ver_node);
                
                vc->prepare_data_ca();
                vc->prepare_data_net();

                
                for (auto it = vc->m_version_nodes.begin(); it != vc->m_version_nodes.end(); ++it)
                {
                    if (it->enable == "1")
                    {
                        bool bl = vc->send_update_confirm_request(it->ip, it->port);
                        
                        if (bl)
                        {
                            
                            cout << "收到确认，更新配置文件！" << endl;
                            
                        }
                        else
                        {
                            cout << "版本更新确认请求错误，等待下次更新！" << endl;
                        }       
                    }
                }
                
                vc->update_configFile();
                
            }
            else if (res == 1)
            {
                cout << "============客户端版本更新失败===========" << endl;
                vc->update_configFile();
            }
            else if (res == -1)
            {
                cout << "============客户端版本错误===========" << endl;
                vc->update_configFile();
            }
            else
            {
                cout << "未知错误...." << endl;
                vc->update_configFile();
            }
        }   
        sleep(60);
    }

}

bool Version_Client::init()
{   
    Config *cfg = Singleton<Config>::get_instance();
    
    
    m_flag = cfg->GetFlag();

    m_local_ip = get_local_ip();

    
    vector<string> vec;
    get_mac_info(vec);
    if (vec.size() != 0)
    {
        m_mac = vec[0];
    }

    
    m_version_nodes = cfg->GetNodeInfo_s();

    if (m_flag == 0)
    {
        
        
        m_version = ca_version();
        return true;
    }
    else if(m_flag == 1)
    {
        
        m_target_version = cfg->GetTargetVersion();
        m_version = m_const_version = ca_version();
        return true;
    }
    else
    {
        return false;
    }   
}


string Version_Client::get_local_ip()
{
    std::ifstream fconf;
    std::string conf;
    std::string tmp = NETCONFIG;

    if (access(tmp.c_str(), 0) < 0)
    { 
        error("Invalid file...  Please check if the configuration file exists!");
        return "";
    }
    fconf.open(tmp.c_str());
    nlohmann::json m_Json;
    fconf >> m_Json;
    fconf.close();

    return m_Json["var"]["local_ip"].get<std::string>();
}

int Version_Client::compare_version()
{
    
    cout << "对比目标版本：" << endl;
    cout << "m_const_version：" << m_const_version << endl;
    cout << "m_target_version：" << m_target_version << endl;
    int res = -2;
    if (m_const_version == m_target_version)
    {   

        res = 0;
    }
    
    if (m_const_version < m_target_version)
    {
        res = 1;
    }

    if (m_const_version > m_target_version)
    {   
        res = -1;
    }
    
    return res;
}




int Version_Client::get_mac_info(vector<string> &vec)
{
    int fd;
    int interfaceNum = 0;
    struct ifreq buf[16] = {0};
    struct ifconf ifc;
    char mac[16] = {0};

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");

        close(fd);
        return -1;
    }
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t)buf;
    if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc))
    {
        interfaceNum = ifc.ifc_len / sizeof(struct ifreq);
        while (interfaceNum-- > 0)
        {
            
            if(string(buf[interfaceNum].ifr_name) == "lo")
            {
                continue;
            }
            if (!ioctl(fd, SIOCGIFHWADDR, (char *)(&buf[interfaceNum])))
            {
                memset(mac, 0, sizeof(mac));
                snprintf(mac, sizeof(mac), "%02x%02x%02x%02x%02x%02x",
                    (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[0],
                    (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[1],
                    (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[2],

                    (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[3],
                    (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[4],
                    (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[5]);
                
                std::string s = mac;
                vec.push_back(s);
            }
            else
            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                close(fd);
                return -1;
            }
        }
    }
    else
    {
        printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}


int Version_Client::get_node_version_info(node_version_info &currNode)
{
    currNode.mac = m_mac;
    currNode.version = m_version;
    currNode.local_ip = m_local_ip;

    return 0;
}


void Version_Client::prepare_data_ca()
{   
    cstring *s = cstr_new_sz(0);
    cstring *smac = cstr_new(m_mac.c_str());
    cstring *sversion = cstr_new(m_version.c_str());
    cstring *sip = cstr_new(m_local_ip.c_str());
    
    
	
	

    ser_varstr(s, smac);
    ser_varstr(s, sversion);
    ser_varstr(s, sip);
    
    cstring * send = message_str((const unsigned char*)"\xf9\xbe\xb4\xd9", UPDATEREQUEST, s->str, s->len);
    
    m_request_data = string(send->str, send->len);
   
    
 
    
    

    cstr_free(s, true);
    cstr_free(send, true);
}

static atomic<unsigned int> serial;
void Version_Client::prepare_data_net()
{    
    string data = m_request_data;
    
    net_pack pack;

	pack.len = (int)data.size();
	pack.data = data;
	
	pack.len = sizeof(int)*2 + pack.data.size() + 32;
    char version[32] = {0};
	srand( (unsigned)time(NULL) + serial++ );
	for (int i = 0; i < 32 - 1; i++)
		version[i] = (rand() % 9) + 48;
	version[32 - 1] = '\0';
    pack.version = string(version, 32);

	
    int pack_len = pack.len + sizeof(int);
	char* buff = new char[pack_len];
	ExitCaller ec([=] { delete[] buff; });
	
    
	memcpy(buff, &pack.len, 4);
	memcpy(buff + 4, &pack.type, 4);
	memcpy(buff + 4 + 4, &pack.len, 4);
	memcpy(buff + 4 + 4 + 4, pack.data.data(), pack.len);
	memcpy(buff + 4 + 4 + 4 + pack.len, pack.version.data(), 32);
	m_request_data = string(buff, pack_len);
}


bool Version_Client::send_verify_version_request()
{   
    
    
    for (auto it = m_version_nodes.begin(); it != m_version_nodes.end(); ++it)
    {
        if (it->enable == "1")
        {   
            
            TcpSocket socket;
            cout << "准备连接版本服务器 ---->  ip: " << it->ip << ", port: " << it->port << endl;
            int ret = socket.connectToHost(it->ip, it->port);
            if (ret != 0)
            {
                cout << "连接版本服务器失败" << endl;
                socket.disConnect();
                
                continue;
            }
            else
            {   
                cout << "连接版本服务器成功" << endl;
                cout << "开始发送数据：=====》" << endl;
                
                ret = socket.sendMsg(m_request_data);
                if (ret != 0)
                {
                    cout << "发送数据失败" << endl;
                    socket.disConnect();
                    
                    continue;
                }
                else
                {
                    cout << "发送数据成功" << endl;
                    
                    cout << "开始接收数据" << endl;
                    string res = socket.recvMsg();
                    
                    if (res == "")
                    {
                        cout << "接收数据超时或者异常！" << endl;
                        socket.disConnect();
                        sleep(1);
                        continue;
                    }
                    else if (res == "Latest")
                    {
                        cout << "当前版本已经是最新版本！" << res << endl;
                        socket.disConnect();
                        return true;
                    }
                    else 
                    {   
                        cout << "接收到的返回数据：" <<res << endl;
                        
                        stringstream ss(res);
                        ss >> m_target_version;
                        ss >> m_url;
                        ss >> m_hash;

                        cout << "目标版本：" << m_target_version << endl;
                        cout << "下载地址：" << m_url << endl;
                        cout << "验证hash值：" << m_hash << endl;

                        if (verify_data())
                        {
                            cout << "数据验证成功！" << endl;
                            cout << "开始下载最新版本==》" << endl;
                            if (download_latest_version())
                            {
                                cout << "下载成功!" << endl;
                                
                                
                                m_version_server_ip = it->ip;

                                
                                socket.disConnect();
                                start_shell_script();
                                
                                return true;
                            }
                        }
                        else
                        {
                            cout << "验证数据失败！" << endl;
                            socket.disConnect();
                            return false;
                        }
                    }
                    
                }
            
            }
        } 
        
        
    }

    return false;
}

bool Version_Client::send_update_confirm_request(string ip, unsigned short port)
{
    
    
    
    TcpSocket socket;
    int ret = socket.connectToHost(ip, port);
    if (ret != 0)
    {
        cout << "连接版本服务器失败" << endl;
        socket.disConnect();
        return false;
    }
    
    
    ret = socket.sendMsg(m_request_data);
    if (ret != 0)
    {
        cout << "发送数据失败" << endl;
        socket.disConnect();
        return false;
    }
    cout << "发送数据成功" << endl;

    
    string res = socket.recvMsg();
    socket.disConnect();
    if (res == "")
    {
        cout << "接收数据超时或者异常！" << endl;
        return false;
    }
    else if (res == "Latest")
    {
        cout << "当前版本已经是最新版本！" << res << endl;
        return true;
    }
    else 
    {   
        cout << "版本更新失败，等待下次更新" << endl;
        return false;    
    }    

    return false;
}



bool Version_Client::verify_data()
{
    string tmp = m_target_version + m_url;
    string hash = getsha256hash(tmp);
    if (m_hash == hash)
    {   
        debug("数据验证成功，数据完整！");
        return true;
    }
    else
    {
        debug("数据验证失败，数据有误！");
        return false;
    }  
}


bool Version_Client::download_latest_version()
{
    debug("开始下载最新版本...");
    cout << "正在下载最新版本..." << endl;

    HttpRequest ImageReq(m_url);
    ImageReq.setRequestMethod("GET");
    ImageReq.setRequestProperty("Cache-Control", "no-cache");
    ImageReq.setRequestProperty("Content-Type", "application/octet-stream");
    ImageReq.setRequestProperty("Connection", "close\r\n");

    ImageReq.connect();
    ImageReq.send();
    ImageReq.handleRead();
    ImageReq.downloadFile("./ebpc.tar.gz");

    debug("最新版本下载完成...");
    cout << "最新版本下载完成..." << endl;
    return true;
}


bool Version_Client::start_shell_script()
{
    debug("准备安装程序，启动更新脚本...");
    if(Singleton<Config>::get_instance()->UpdateConfig(1, m_target_version, m_version_server_ip))
    {
        cout << "配置文件更新完毕，下一步解压文件！" << endl;
        system("tar zxvf ebpc.tar.gz");
        
        system("chmod 755 update.sh");
        system("./update.sh");
        
        exit(0);
    }
    
    return false; 

}


bool Version_Client::get_version_server_info(node_info &version_server)
{   
    
    m_version_server_ip = Singleton<Config>::get_instance()->GetVersionServer();
    for (auto it = m_version_nodes.begin(); it != m_version_nodes.end(); ++it)
    {
        if (it->ip == m_version_server_ip && it->enable == "1")
        {
            version_server = *it;
            return true;
        }
    }
    
    return false;
}


bool Version_Client::update_configFile()
{
    if(Singleton<Config>::get_instance()->UpdateConfig(0, "", "", m_const_version))
    {
        return true;
    }
    
    return false;      
}


int Version_Client::get_flag()
{
    return m_flag;
}
