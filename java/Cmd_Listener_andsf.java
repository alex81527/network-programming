/*
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   - Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   - Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *   - Neither the name of Oracle or the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.Date;
import java.util.Random;
import java.util.Timer;
import java.util.TimerTask;

import org.jdom2.Document;
import org.jdom2.output.XMLOutputter;

public class Cmd_Listener {
	private static String ANDSF_SERVER="140.113.240.176";
    	private static int ANDSF_SERVER_PORT=38765;
    	private static String WIFI_INTERFACE="wlan5";
	private static String UE_IP="140"; 
	private static boolean D=true;
    	private static Socket socket_;
	private static PrintWriter out;
	private static BufferedReader in;
	private static Random ran = new Random();
	private static String LTE_MAC;
	private static Timer time;
	
	

	private static class SchedulerTask extends TimerTask {
		public void run(){
			try {
				lte();
				System.out.println("Sending LTE signal measurements to ANDSF server at " + new Date());
				andsf_response();
			} catch(Exception e) {
				e.printStackTrace();
			}
		}
	}
	public static void lte(){
	
		XMLOutputter outputter = new XMLOutputter();
                ByteArrayOutputStream ClientInitMessage = new ByteArrayOutputStream() ;

		

		String RSSI = String.valueOf(-ran.nextInt(100));
		String[] UEData = {"10",RSSI,LTE_MAC,UE_IP,"0"};

                ClientSyncML a = new ClientSyncML();
                Random random = new Random() ;
                random.setSeed(System.currentTimeMillis()) ; //set seed
                String SessionID = Integer.toString(random.nextInt(1000));
                String MsgID = Integer.toString(random.nextInt(1000));
                a.setSessionID(SessionID);
                a.setMsgID(MsgID);
                a.setSourcelocURI("IMEI:"+LTE_MAC);
                a.setTargetlocURI(ANDSF_SERVER);
                a.setSetupHdr();
                a.setSetupBody();
                a.setClientInfo(UEData);


		Document document = new Document(a.getSyncMLmessage());
		
		try {
			System.out.println("clientInitRequest:");
			outputter.output(document, System.out);
			System.out.println("********************************************");
			outputter.output(document, ClientInitMessage);
			
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		
		out.println(ClientInitMessage.toString());
		out.flush();
	

	

	}

	public static void andsf_response() throws IOException{
		System.out.println("ANDSF_Response:");
		System.out.println(client_in.readLine());
		System.out.println(client_in.readLine());
		System.out.println("***************************");
	}

	public static String[][] scan_wifi() throws IOException{

    		Process p ;

		BufferedReader stdInput;

		String rssi ;

		String mac ;

		String ssid ;

		String s;
		String[][] WifiData = new String[100][3];
		
		int n=0;

    	 

		p = Runtime.getRuntime().exec("sudo iwlist "+WIFI_INTERFACE+" scan");

		stdInput = new BufferedReader(new InputStreamReader(p.getInputStream()));

		//ss = stdInput.readLine();	

		//String[] lines = ss.split("\n");
		//for(String s: lines) 
		while( (s=stdInput.readLine())!=null)
		{
			//s.replaceFirst(".*\n",""); 
			//System.out.println(s+"\n");
			if(s.matches(".*Address.*")){

				s = s.replaceAll(".*: ", "");
				//System.out.println("+++"+s);
				mac = s;

			}

			else if(s.matches(".*Signal level.*")){

				s = s.replaceAll(".*=", "");

				s = s.replaceAll(" dBm", "");

				rssi=s;

			}

			else if(s.matches(".*ESSID.*")){

				s = s.replaceAll(".*:", "");

				s = s.replaceAll("\"", "");

				ssid=s;
				WifiData[n][0]= ssid;
				WifiData[n][1]= mac;
				WifiData[n][2]= rssi;
				n++;
				
						//={ssid,mac,rssi};
			}

		}

    	

		System.out.println(ssid);

		System.out.println(mac);

		System.out.println(rssi);

		System.out.println(LTE_MAC);

		System.out.println(UE_IP);
/*
		out.println("2");
		out.println(String.valueOf(n));
		out.println(ssid.toString());	
		out.println(mac.toString());
		out.println(rssi.toString());
		out.println(LTE_MAC);
		out.println("10.100.10.45");
*/		
		
		return WifiData;
		
		//send data to ANDSF server
		
		//ANDSF_Client("scan_wifi", "2", String.valueOf(n), ssid.toString(), mac.toString(), rssi.toString(), "LTE_MAC", "LTE_IP");

 
    	}

	public static String waitServerRequest() throws IOException{
		String str = in.readLine();
		
		//System.out.println("waitServerRequest1:");
		//System.out.println("000"+str+".");
		str += in.readLine();
		//System.out.println("111"+str+".");
		str += in.readLine();
		//System.out.println("waitServerRequest2:");
		//System.out.println(str);
		//System.out.println("***************************");
		
		return str;
	}

	public static void clientResponse(String serverMessage) {

		XMLOutputter outputter = new XMLOutputter();
		ClientXMLParser parser = new ClientXMLParser(serverMessage);
		ClientSyncML clientSyncML = new ClientSyncML();
		ByteArrayOutputStream clientMessage = new ByteArrayOutputStream(); 
		String targetLocURI = parser.getTargetlocURI();
		String sourceLocURI = parser.getSourcelocURI();
		String sessionID    = parser.getSessionID();
		String msgID    = parser.getMsgID();
		String[][] requestData = parser.getRequestData();
		
		clientSyncML.setMsgID(msgID);
		clientSyncML.setSessionID(sessionID);
		clientSyncML.setSourcelocURI(targetLocURI);
		clientSyncML.setTargetlocURI(sourceLocURI);
		
		
		
		System.out.println("ClientParseData");
		for(int i = 0;i<requestData.length;i++){
			if(requestData[i][0]==null) break;
			for(int j = 0;j<requestData[i].length;j++){
				if(requestData[i][j]==null) break;
				System.out.print(requestData[i][j]+" ");
				
			}
			System.out.println();
		}
		System.out.println("************************");
	
		int scenario = 0;
		String[][] data = null;
		if(requestData[0][2].equals("./UE_Location/WLAN_Location")){      //Wi-Fi Mode
				
			data = getWLAN_Location();
			scenario = 1;
		}
		
		String[] responseData2 = {requestData[0][0],requestData[0][1],"200"};
		clientSyncML.setStatusCommand(responseData2);
		clientSyncML.setResultCommand(scenario,requestData[0][0],data);	
		 
		
		
		Document document = new Document(clientSyncML.getSyncMLmessage());
		try {
			System.out.println("clientResponseRequest:");
			outputter.output(document, System.out);
			System.out.println("********************************************");
			clientMessage = new ByteArrayOutputStream(); 
			outputter.output(document, clientMessage);
			
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	
		out.println(clientMessage.toString());
		out.flush();
		
		
	}

	public static void clientFinish(String serverMessage) {
		
		XMLOutputter outputter = new XMLOutputter();
		ClientXMLParser parser = new ClientXMLParser(serverMessage);
		ClientSyncML clientSyncML = new ClientSyncML();
		ByteArrayOutputStream clientMessage = new ByteArrayOutputStream(); 
		String targetLocURI = parser.getTargetlocURI();
		String sourceLocURI = parser.getSourcelocURI();
		String sessionID    = parser.getSessionID();
		String msgID    = parser.getMsgID();
		String[][] requestData = parser.getRequestData();
		
		clientSyncML.setMsgID(msgID);
		clientSyncML.setSessionID(sessionID);
		clientSyncML.setSourcelocURI(targetLocURI);
		clientSyncML.setTargetlocURI(sourceLocURI);
		
		System.out.println("ClientParseData");
		for(int i = 0;i<requestData.length;i++){
			if(requestData[i][0]==null) break;
			for(int j = 0;j<requestData[i].length;j++){
				if(requestData[i][j]==null) break;
				System.out.print(requestData[i][j]+" ");
				
			}
			System.out.println();
		}
		System.out.println("************************");
		
		
		String = APMAC = requestData[0][2];
		String = APSSID = requestData[0][3];
		connectWiFi(APMAC,APSSID);		


		String[] responseData2 = {requestData[0][0],requestData[0][1],"200"};
		clientSyncML.setStatusCommand(responseData2);
		
		Document document = new Document(clientSyncML.getSyncMLmessage());
		try {
			System.out.println("clientFinishRequest:");
			outputter.output(document, System.out);
			System.out.println("********************************************");
			clientMessage = new ByteArrayOutputStream(); 
			outputter.output(document, clientMessage);
			
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	
		out.println(clientMessage.toString());
		out.flush();
		
	}

	public static String[][] getWLAN_Location(){
		//String[][] WifiData = {{"AP1","40:16:7E:B9:44:6A","-11"},{"AP2","00:01:02","-22"},{"AP3","00:01:03","-33"}};
		String[][] WifiData = scan_wifi();
		return WifiData;
	}

	public static void connectWiFi(String APMAC,String APSSID){
	
		Runtime.getRuntime().exec(String.format("%s %s %s %s","sudo ./handover.sh", WIFI_INTERFACE, APSSID, "wirelab020"));
	}
	
	public static void main(String[] args) throws IOException {
		
		int portNumber = 8765;
    		try {
    			if(D){ System.out.println("[Cmd_Listener] Creating socket"); }
    			socket_ = new Socket(ANDSF_SERVER, ANDSF_SERVER_PORT);
    			if(D){ System.out.println("[Cmd_Listener] socket ok"); }
			out = new PrintWriter(socket_.getOutputStream(),true);
			in = new BufferedReader(new InputStreamReader(socket_.getInputStream()));
    		
			//Process p = Runtime.getRuntime().exec("ifconfig eth2 | grep HWaddr | sed s/'.*HWaddr '//g");	
			//BufferedReader stdInput = new BufferedReader(new InputStreamReader(p.getInputStream()));
			LTE_MAC ="AB:CD:EF:GH:IJ:KL" ;//stdInput.readLine();
			
			time = new Timer();
			SchedulerTask st = new SchedulerTask();
			time.schedule(st, 0, 2000); //repeat every 2 sec			
			//init_wifi();
    			
    			
    			while(true){
				//package 0
				String serverRequest = waitServerRequest();
				System.out.println("ServerRequest1");
				System.out.println(serverRequest);
				System.out.println("**************************************");
				//package 1:send ue info
				lte();
				//package 2:recevie andsf command
				serverRequest = waitServerRequest();
				System.out.println("ServerRequest2");
				System.out.println(serverRequest);
				System.out.println("**************************************");
				//package3:send wifi/wifi-direct info
				clientResponse(serverRequest);
				//package4:recevie andsf command
				serverRequest = waitServerRequest();
				//package3:send finish 
				clientFinish(serverRequest);
				//package4:recevie andsf command
				serverRequest = waitServerRequest();
				System.out.println("ServerRequest3");
				System.out.println(serverRequest);
				System.out.println("**************************************");


	    			/*if(input.equals("1")){
					System.out.println("Received 1 --> Starting scan_wifi()");
					time.cancel();					
					scan_wifi();
					input = in.readLine();
					if(input.equals("1")){
						System.out.println("Received 1 --> Preparing to handover");
						String bssid = in.readLine();
						String ssid = in.readLine();
						System.out.println("Received bssid --> " + bssid);
						System.out.println("Received ssid --> " + ssid);
						Runtime.getRuntime().exec(String.format("%s %s %s","sudo ./handover.sh wlan4",ssid,"wirelab020"));
					}					
					
	    			}*/
	    		}
    			/*ServerSocket serverSocket = new ServerSocket(portNumber);	
    			while (true) {
    				Socket clientSocket = serverSocket.accept();
    				new Thread(new handler(clientSocket)).start();	
    			}
    			*/
    		} catch (IOException e) {
    			System.out.println("Exception caught when trying to listen on port " + portNumber + " or listening for a connection");
    			System.out.println(e.getMessage());
    		}	

	}
}

