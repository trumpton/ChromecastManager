//
// test.js
//
//

function processform()
{
  var device = document.getElementById("device");
  var devices = device ? device.value : "" ;
  var command = document.getElementById("command");
  var commands = command ? command.value : "" ;

  var url="/" + commands + 
           "?device=" + encodeURIComponent(devices) ;
  var body = null ;
  var method = "GET" ;
  var headers = { } ;
  var responsetxt="<p><b>No response received</b></p>" ;

  resetresponseview() ;
  document.getElementById("rawsent").innerHTML = "<p><pre>{}</pre></p>" ;
  document.getElementById("response").innerHTML = "<p>processing ...</p>" ;
  document.getElementById("rawrecv").innerHTML = "<p><pre>{}</pre></p>" ;

  if (commands == "devicelist") {

    /////////////////////////////////
    // Prepare /devicelist GET

    url="/devicelist" ;
    body= null ;
    
  } else if (commands == "jsonquery" ) {

    /////////////////////////////////
    // Prepare /jsonquery request POST

    var namespace = document.getElementById("namespace");
    var namespaces = namespace ? namespace.value : "" ;
    var receiver = document.getElementById("receiver");
    var receivers = receiver ? receiver.value : "" ;
    var sender = document.getElementById("sender");
    var senders = sender ? sender.value : "" ;
    var message = document.getElementById("message") ;

    try {

      var messagej = JSON.parse(message.value) ;


      headers['Content-Type'] = "application/json" ;

      method = "POST" ;

      body = {
        "namespace": namespaces,
        "sender": senders,
        "receiver": receivers,
        "message": messagej
      } ;

    } catch (e) {

      url = null ;
      responsetxt = "<p><b>Error parsing JSON - request not sent:</b></p>" +
                    "<p><b><i>" + e + "</i></b></p>"

      document.getElementById("response").innerHTML = responsetxt ;

    }

  } else {

    /////////////////////////////////
    // Prepare other ones

    var volume = document.getElementById("volume");
    var volumef = volume ? volume.value : 0.0 ;
    var sleep = document.getElementById("sleep");
    var sleepi = sleep ? sleep.value : 0 ;
    var title = document.getElementById("title");
    var titles = sleep ? title.value : "" ;
    var uri = document.getElementById("uri");
    var uris = uri ? uri.value : "" ;

    if (volume && volume.value != "" && volumef>=0) {
      url = url + "&volume=" + volumef ;
    }

    if (uri && sleep && title && uris!="") {
      url = url + "&uri=" + encodeURIComponent(uris) ;
      if (sleep.value != "") {
        url = url + "&sleep=" + sleepi ;
      }
      if (titles!="") {
        url = url + "&title=" + encodeURIComponent(titles) ;
      }
    }

  }

  /////////////////////////////////
  // Perform the fetch

  if (url) {

    var xhr = new XMLHttpRequest();

    var jsonrequest = {
      "Accept": "application/json, text/plain, */*",
      "cache": "no-cache",
      "method": method
    } ;
    if (Object.keys(headers).length>0) jsonrequest['headers'] = headers ;
    if (body) jsonrequest['body'] = JSON.stringify(body) ; 

    var pretty = body ? "<pre>" + JSON.stringify(body, null, 4) + "</pre>" : "<p><i>no body has been sent</i></p>" ;

    document.getElementById("rawsent").innerHTML = 
                "<p><b>URL: " + url + "</b></p>" + pretty + "</p>" ;

    fetch( url , jsonrequest )

  /////////////////////////////////
  // Parse the Response

    .then( response => response.text() )

    .then( text => {

      var json = null ;
      document.getElementById("rawrecv").innerHTML = "<p><pre>" + 
                                                     text ? text : "<i>no body received</i>" + 
                                                     "</pre></p>" ;

      try {
        
        json = JSON.parse(text) ;

      } catch (e) {

        json = null ;

        responsetxt = "<p><b>Error receiving response:</b></p>" +
                      "<p><pre>" + text + "</pre></p>" ;

      }

      if (json) {

        var pretty = "<pre>" + JSON.stringify(json, null, 4) + "</pre>"  ;

        document.getElementById("rawrecv").innerHTML = "<p>" + pretty + "</p>" ;

        var status = json.status ;         
        var namespace = json.namespace ;

        if (!body) {

          // No body supplied in request, just provide response

          var pretty = "<pre>" + JSON.stringify(json, null, 4) + "</pre>" ;
          responsetxt = "<p>" + pretty + "</p>" ;

        } else if (status) {

          // Request is a 'macro' so parse response

          var pretty = "<pre>" + JSON.stringify(json, null, 4) + "</pre>" ;
          responsetxt = "<p>" + pretty + "</p>" ;

        } else if (namespace) {

          // Request was a /jsonrequest, so parse the response

          var sender = json.sender ;
          var receiver = json.receiver ;
          var message = json.message ;

          if (!namespace || !sender || !receiver || !message) {

            errmsg="Missing namespace / sender / receiver / message from response" ;

          } else {

            var pretty = "<pre>" + JSON.stringify(message, null, 4) + "</pre>" ;

            responsetxt = "<p><b>namespace:</b> " + namespace + "</p>" +
                          "<p><b>sender:</b> " + sender +"</p>" +
                          "<p><b>receiver:</b> " + receiver + "</p>" +
                          "<p>" + pretty + "</p>" ;
          }

        }

      }

      document.getElementById("response").innerHTML = responsetxt ;

    })

    .catch((error) => {

      responsetxt = "<p><b>Error processing request:</b></p>" +
                    "<p><b><i>" + error + "</i></b></p>" +
                    "</pre></p>" ;

      document.getElementById("response").innerHTML = responsetxt ;

    });

  }

}

function enablefields()
{
  var command = document.getElementById("command");
  var commands = command ? command.value : "" ;
  var deviced = document.getElementById("deviced");
  var jsonqueryd = document.getElementById("jsonqueryd");
  var commandd = document.getElementById("commandd");

  deviced.style.display = "none" ;
  jsonqueryd.style.display = "none" ;
  commandd.style.display = "none" ;

  if (commands=="devicelist") {
    // Nothing to do
  } else if (commands=="jsonquery") {
    deviced.style.display = "block";
    jsonqueryd.style.display = "block" ;
  } else if (commands=="command") {
    deviced.style.display = "block";
    commandd.style.display = "block" ;
  }
}


function populatejson(i)
{
  var message = document.getElementById("message") ;
  var namespace = document.getElementById("namespace") ;
  if (!message || !namespace) return ;
  switch(i) {

  case 1: namespace.value = "urn:x-cast:com.google.cast.connection" ;
          message.value = 
            "{\n" +
            "  \"type\" : \"CONNECT\"\n" +
            "}" ;
          break ;

  case 2: namespace.value = "urn:x-cast:com.google.cast.receiver" ;
          message.value = 
            "{\n  \"requestId\" : 1,\n" +
            "  \"type\" : \"GET_STATUS\"\n" +
            "}" ;
          break ;

  case 3: namespace.value = "urn:x-cast:com.google.cast.receiver" ;
          message.value =
            "{\n" +
            "  \"requestId\" : 1,\n" +
            "  \"type\" : \"SET_VOLUME\",\n" +
            "  \"volume\" : {\n" +
            "    \"level\" : 0.1\n" +
            "  }\n" +
            "}" ;
             break ;

  case 4: namespace.value = "urn:x-cast:com.google.cast.receiver" ;
          message.value =
            "{\n" +
            "  \"type\": \"LAUNCH\",\n" +
            "  \"requestId\": 2,\n" +
            "  \"appId\": \"CC1AD845\"\n" +
            "}" ;
            break ;

  case 5: namespace.value = "urn:x-cast:com.google.cast.media" ;
          message.value = 
            "{\n" +
            "  \"requestId\": 1,\n" +
            "  \"type\": \"LOAD\",\n" +
            "  \"autoplay\": true,\n" +
            "  \"media\": {\n" +
            "    \"contentId\": \"http://live7.avf.ch:8000/ipmusicslow320\",\n" +
            "    \"streamType\": \"LIVE\",\n" +
            "    \"contentType\": \"audio/mpeg\"\n" +
            "  }\n" +
            "}" ;
          break ;
/*

http://www.vizier.uk/mediafiles/Test/440hz.mp3

This is the openhab binding which has the launch / load calls

https://github.com/alexreinert/openhab2-addons/blob/master/addons/binding/org.openhab.binding.chromecast/src/main/java/org/openhab/binding/chromecast/internal/ChromecastCommander.java

This git repository is the underlying chromecast library used by openhab
In the middle of it should be chromeCast.load(title, null, url, mimeType)

https://github.com/vitalidze/chromecast-java-api-v2/blob/master/src/main/java/su/litvak/chromecast/api/v2/Channel.java

*/

  case 6: namespace.value = "urn:x-cast:com.google.cast.media" ;
          message.value =
            "{\n" +
            "  \"type\": \"PAUSE\",\n" +
            "  \"mediaSessionId\": \"media_session_id\",\n" +
            "  \"requestId\": 2\n" +
            "}" ;
             break ;

  default:message.value = "{}" ;
          break ;
  }
}

function toggleview(id1, id2)
{
  var choice1= document.getElementById(id1) ;
  var choice2= id2 ? document.getElementById(id2) : null ;  
  if (choice1.style.display == "block") {
    choice1.style.display = "none" ;
    if (choice2) choice2.style.display = "block" ;
  } else {
    choice1.style.display = "block" ;
    if (choice2) choice2.style.display = "none" ;
  }
}

function resetresponseview()
{
  var rawsent=document.getElementById('rawsent') ;
  var response=document.getElementById('response') ;
  var rawrecv=document.getElementById('rawrecv') ;
  rawsent.style.display = "none" ;
  response.style.display = "block" ;
  rawrecv.style.display = "none" ;
}

function resethelpview()
{
  document.getElementById('overview').style.display="block" ;
  document.getElementById('launching').style.display="none" ;
  document.getElementById('responsecodes').style.display="none" ;
  document.getElementById('envvars').style.display="none" ;
  document.getElementById('queryoptions').style.display="none" ;
  document.getElementById('gettransactions').style.display="none" ;
  document.getElementById('posttransactions').style.display="none" ;
  document.getElementById('usefullinks').style.display="none" ;
}




function refreshdevicelist(tag)
{

  fetch( "/devicelist" , null )

  .then( response => response.text() )

  .then( text => {

    try {        

      var json = JSON.parse(text) ;

      if (json.devicelist) {
    
        // add to select
        var select=document.getElementById(tag) ;

        if (!selecthasoption(select, "")) {
          var option=document.createElement("option") ;
          option.text="" ;
          select.add(option) ;
        }

        for (i=0; i<json.devicelist.length; i++) {

          if ( json.devicelist[i].friendlyName && 
               !selecthasoption(select, json.devicelist[i].friendlyName) ) {

            var option=document.createElement("option") ;
            option.text=json.devicelist[i].friendlyName ;
            select.add(option) ;

          }
    
        }

      }

    } catch (e) {

    }
    
  }) ;

}


function selecthasoption(select, value)
{
  if ( !select || !select.options || !select.options.length ) return false ; 

  var len = select.options.length ;

  for (var i=0; i<len; i++) {

    if (select.options[i].text == value) return true ;

  }

  return false ;
}


