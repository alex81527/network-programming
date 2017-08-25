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
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Timer;
import java.util.TimerTask;
import java.util.Date;

public class Cmd_Listener {
	private static class handler implements Runnable{
		private Socket skt;
		private PrintWriter out;
		private BufferedReader in;
		private String input;
		private Timer t;
		public handler( Socket s, Timer time){
			skt = s;
			t = time;
		}
		public void run(){
			try{
				out = new PrintWriter(skt.getOutputStream(),true);
				in = new BufferedReader(new InputStreamReader(skt.getInputStream()));
				String input = in.readLine();

				if(input.equals("1")) {
					System.out.println(skt.getRemoteSocketAddress()+ "-->" + input);
					String cmd = String.format("%s %s","./sed.sh", "scan_wifi");	
					Runtime.getRuntime().exec(cmd);
                                        System.out.println("Starting " + cmd);
					System.out.println("Waiting for response...");
					String res = in.readLine();
					System.out.println(skt.getRemoteSocketAddress()+ "-->" + res);
					// "1" means UE has to handover from LTE to WiFi
					if(res.equals("1")){
						String bssid = in.readLine();
                                                String ssid = in.readLine();
						System.out.println(skt.getRemoteSocketAddress()+ "-->" + bssid);
						System.out.println(skt.getRemoteSocketAddress()+ "-->" + ssid);
						String cmd2 = String.format("%s %s %s %s","./connect_wifi.sh wlan1", ssid, "gantoegantoe", bssid);
                                        	Runtime.getRuntime().exec(cmd2);
                                        	System.out.println("Starting " + cmd2);
						t.cancel();
					}
				}
				else if(input.equals("3")) {
					String target_mac = in.readLine();
					String target_ip = in.readLine();
				
					System.out.println(skt.getRemoteSocketAddress()+ "-->" + input);
					System.out.println(skt.getRemoteSocketAddress()+ "-->" + target_mac);
					System.out.println(skt.getRemoteSocketAddress()+ "-->" + target_ip);
				
					String cmd = String.format("%s %s %s","./connect_wifi_direct.sh", target_mac, target_ip);
					Runtime.getRuntime().exec(cmd);
					System.out.println("Starting " + cmd);
				}
			}
			catch(IOException e){
				e.printStackTrace();
			}
		}
	}
	
	private static class SchedulerTask extends TimerTask {
		public void run(){
			try {
				String cmd = String.format("%s","./sed.sh lte");
				Runtime.getRuntime().exec(cmd);
				System.out.println("Sending LTE signal measurements to ANDSF server at " + new Date());
			} catch(Exception e) {
				e.printStackTrace();
			}
		}
	}
	
	
	public static void main(String[] args) throws IOException {
	
	int portNumber = 8765;
	
		try {
			Timer time = new Timer();
			SchedulerTask st = new SchedulerTask();
			time.schedule(st, 0, 2000); //repeat every 2 sec
			
	
			ServerSocket serverSocket = new ServerSocket(portNumber);
		
			while (true) {
				Socket clientSocket = serverSocket.accept();
				new Thread(new handler(clientSocket, time)).start();	
			}
		} catch (IOException e) {
			System.out.println("Exception caught when trying to listen on port " + portNumber + " or listening for a connection");
			System.out.println(e.getMessage());
		}
	}
}
