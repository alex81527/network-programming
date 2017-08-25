/*
 * Copyright (c) 1995, 2013, Oracle and/or its affiliates. All rights reserved.
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

import java.io.*;
import java.net.*;

public class ANDSF_Client {
	
	public static void main(String[] args) throws IOException {
        /*
        if (args.length < 4) {
            System.err.println("Usage: java ANDSF_Client <host name> <port number> <MAC_ADDR> <IP_ADDR>");
            System.exit(1);
        }*/
	
        String hostName = args[0];
        int portNumber = Integer.parseInt(args[1]);
        String type = args[2];
	
	Socket kkSocket = new Socket(hostName, portNumber);
        PrintWriter out = new PrintWriter(kkSocket.getOutputStream(), true);
        BufferedReader in = new BufferedReader(new InputStreamReader(kkSocket.getInputStream()));
        try{
		if(type.equals("lte")) {
			String LTE_PID=args[3];
			String LTE_RSSI=args[4];
			String MY_LTE_IP=args[5];
			String MY_LTE_MAC=args[6];
			String PADDING=args[7];
			out.println("1");
			out.println(LTE_PID);
			out.println(LTE_RSSI);
			out.println(MY_LTE_MAC);	
			out.println(MY_LTE_IP);
			out.println(PADDING);
		}
        	else if(type.equals("scan_wifi")){
		BufferedReader br;
		int n=0;	
		
		out.println("2");
		br = new BufferedReader(new FileReader("ssid.txt"));
    		try {
        		StringBuilder sb = new StringBuilder();
        		String line = br.readLine();
        		while (line != null) {
				n++;
            			sb.append(line);
            			sb.append(System.lineSeparator());
            			line = br.readLine();
        		}
        		String everything = sb.toString();
			System.out.println(everything);
			out.println(n); 
			out.println(everything);
    		} finally {
        		br.close();
    		}

        	br = new BufferedReader(new FileReader("mac.txt"));
    		try {
        		StringBuilder sb = new StringBuilder();
        		String line = br.readLine();
        		while (line != null) {
            			sb.append(line);
            			sb.append(System.lineSeparator());
            			line = br.readLine();
        		}
        		String everything = sb.toString();
			System.out.println(everything);
			out.println(everything);
    		} finally {
        		br.close();
    		}

		br = new BufferedReader(new FileReader("rssi.txt"));
    		try {
        		StringBuilder sb = new StringBuilder();
        		String line = br.readLine();
        		while (line != null) {
            			sb.append(line);
            			sb.append(System.lineSeparator());
            			line = br.readLine();
        		}
        		String everything = sb.toString();
			System.out.println(everything);
			out.println(everything);
    		} finally {
        		br.close();
    		}
		
		String MY_LTE_IP=args[3];
		String MY_LTE_MAC=args[4];
		out.println(MY_LTE_MAC);	
		out.println(MY_LTE_IP);
		System.out.println(MY_LTE_MAC);
		System.out.println(MY_LTE_IP);
		}//end if scan_wifi
	
		else if(type.equals("init_wifi")) {
		//System.out.println(args.length);	
	
		String WIFI_MAC = args[3];
		String WIFI_RSSI = args[4];
		String MY_WIFI_IP = args[5];
		String MY_WIFI_MAC = args[6];
		//System.out.println("123");
		String DIRECT_MAC = args[7];
		//System.out.println(target_rssi);
		out.println("1");
		out.println(WIFI_MAC);
		out.println(WIFI_RSSI);
		out.println(MY_WIFI_MAC);
		out.println(MY_WIFI_IP);
		out.println(DIRECT_MAC);
		System.out.println(WIFI_MAC);
		System.out.println(WIFI_RSSI);
		System.out.println(MY_WIFI_MAC);
		System.out.println(MY_WIFI_IP);
		System.out.println(DIRECT_MAC);
/*		String response = in.readLine();
		if(response.equals("3")){
			try {
			    System.out.println("Response: " + response);	
			    String s = null;
			    String mac = in.readLine();
			    String wps = in.readLine();
			    System.out.println("Response: " + mac);
 			    System.out.println("Response: " + wps);
				try {
					File file = new File("direct.txt");
					BufferedWriter w = new BufferedWriter(new FileWriter(file));
					w.write(mac+"\n");
					w.write(wps);
					w.close();
					
				} catch (IOException e) {
					e.printStackTrace();
				}
			    String cmd = String.format("%s %s %s", "./connect_wifi_direct.sh", mac, wps);					
			    // using the Runtime exec method:
			    Process p = Runtime.getRuntime().exec(cmd);
			     
			    BufferedReader stdInput = new BufferedReader(new
				 InputStreamReader(p.getInputStream()));
		 
			    BufferedReader stdError = new BufferedReader(new
				 InputStreamReader(p.getErrorStream()));
		 
			    // read the output from the command
			    System.out.println("Here is the standard output of the command:\n");
			    while ((s = stdInput.readLine()) != null) {
				System.out.println(s);
			    }
			     
			    // read any errors from the attempted command
			    System.out.println("Here is the standard error of the command (if any):\n");
			    while ((s = stdError.readLine()) != null) {
				System.out.println(s);
			    }
			     
			    //System.exit(0);
			}
			catch (IOException e) {
			    System.out.println("exception happened - here's what I know: ");
			    e.printStackTrace();
			    System.exit(-1);
			}
	
		}
	

		
*/
		}// end if init_wifi
		
		} catch (UnknownHostException e) {
		    System.err.println("Don't know about host " + hostName);
		    System.exit(1);
		} catch (IOException e) {
		    System.err.println("Couldn't get I/O for the connection to " + hostName);
		    System.exit(1);
		}

	
	
	}//end of main
}
