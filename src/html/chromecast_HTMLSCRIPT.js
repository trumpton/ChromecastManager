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
    
  } else if (commands == "serverinfo") {

    /////////////////////////////////
    // Prepare /serverinfo GET

    url="/serverinfo" ;
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
                    "<p><i>" + getsampleerror(msgscript.value, e) + "</i></p>"

      document.getElementById("response").innerHTML = responsetxt ;

    }

  } else {

    /////////////////////////////////
    // Append the custom parameters

    var param1 = document.getElementById("param1") ;
    var param1s = param1?param1.value:"" ;
    var value1 = document.getElementById("value1") ;
    var value1s = param1?value1.value:"" ;

    var param2 = document.getElementById("param2") ;
    var param2s = param2?param2.value:"" ;
    var value2 = document.getElementById("value2") ;
    var value2s = param2?value2.value:"" ;

    var param3 = document.getElementById("param3") ;
    var param3s = param3?param3.value:"" ;
    var value3 = document.getElementById("value3") ;
    var value3s = param3?value3.value:"" ;

    if (param1s!="") url = url + "&" + param1s + "=" + encodeURIComponent(value1s) ;
    if (param2s!="") url = url + "&" + param2s + "=" + encodeURIComponent(value2s) ;
    if (param3s!="") url = url + "&" + param3s + "=" + encodeURIComponent(value3s) ;

    var commandvars = document.getElementById("commandvars") ;

    if ( commandvars && commandvars.value != "" ) {

      try {

        body = JSON.parse(commandvars.value) ;
        headers['Content-Type'] = "application/json" ;
        method = "POST" ;

      } catch (e) {

        url = null ;
        responsetxt = "<p><b>Error parsing JSON - request not sent:</b></p>" +
                      "<p><i>" + getsampleerror(commandvars.value, e) + "</i></p>"

        document.getElementById("response").innerHTML = responsetxt ;

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
  medialistd.style.display = "none" ;

  if (commands=="devicelist") {
    // Nothing to do
  } else if (commands=="serverinfo") {
    // Nothing to do
  } else if (commands=="jsonscript") {
    deviced.style.display = "block";
    medialistd.style.display = "block" ;
  } else if (commands=="jsonquery") {
    deviced.style.display = "block";
    medialistd.style.display = "block" ;
  } else {
    deviced.style.display = "block";
    commandd.style.display = "block" ;
    medialistd.style.display = "block" ;
  }
}


function populatejson(i)
{
  var message = document.getElementById("message") ;
  var namespace = document.getElementById("namespace") ;
  var sender = document.getElementById("sender") ;
  var receiver = document.getElementById("receiver") ;

  if (!message || !namespace || !sender || !receiver) return ;

  switch(i) {

  case 1: namespace.value = "urn:x-cast:com.google.cast.tp.connection" ;
          sender.value = "session-0" ;
          receiver.value = "$sessionId" ;
          message.value = 
            "{\n" +
            "  \"type\" : \"CONNECT\"\n" +
            "}" ;
          break ;

  case 2: namespace.value = "urn:x-cast:com.google.cast.receiver" ;
          sender.value = "sender-0" ;
          receiver.value = "receiver-0" ;
          message.value = 
            "{\n  \"requestId\" : 1,\n" +
            "  \"type\" : \"GET_STATUS\"\n" +
            "}" ;
          break ;

  case 3: namespace.value = "urn:x-cast:com.google.cast.receiver" ;
          sender.value = "sender-0" ;
          receiver.value = "receiver-0" ;
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
          sender.value = "sender-0" ;
          receiver.value = "receiver-0" ;
          message.value =
            "{\n" +
            "  \"type\": \"LAUNCH\",\n" +
            "  \"requestId\": 2,\n" +
            "  \"appId\": \"CC1AD845\"\n" +
            "}" ;
            break ;

  case 5: namespace.value = "urn:x-cast:com.google.cast.media" ;
          sender.value = "session-0" ;
          receiver.value = "$sessionId" ;
          message.value = 
            "{\n" +
            "  \"requestId\": 1,\n" +
            "  \"type\": \"LOAD\",\n" +
            "  \"autoplay\": true,\n" +
            "  \"media\": {\n" +
            "    \"contentId\": \"http://www.vizier.uk/mediafiles/Test1.ogg\",\n" +
            "    \"streamType\": \"LIVE\",\n" +
            "    \"contentType\": \"audio/ogg\",\n" +
            "    \"metadata\": {\n" +
            "       \"metadataType\": 0,\n" +
            "       \"title\": \"Test 1 Sample\"\n" +
            "    }\n" +
            "  }\n" +
            "}" ;
          break ;

  case 6: namespace.value = "urn:x-cast:com.google.cast.media" ;
          sender.value = "session-0" ;
          receiver.value = "$sessionId" ;
          message.value =
            "{\n" +
            "  \"type\": \"PAUSE\",\n" +
            "  \"mediaSessionId\": \"media_session_id\",\n" +
            "  \"requestId\": 2\n" +
            "}" ;
             break ;

  case 7: namespace.value = "urn:x-cast:com.google.cast.media" ;
          sender.value = "session-0" ;
          receiver.value = "$sessionId" ;
          message.value = 
            "{\n  \"requestId\" : 1,\n" +
            "  \"type\" : \"GET_STATUS\"\n" +
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
            option.value=json.devicelist[i].friendlyName ;
            select.add(option) ;

          }
    
        }

      }

    } catch (e) {

    }
    
  }) ;

}


function refreshserverinfo(selecttag, mediatag)
{

  fetch( "/serverinfo" , null )

  .then( response => response.text() )

  .then( text => {

    try {        

      var json = JSON.parse(text) ;

      if (json && json.scripts) {
    
        // add to select

        var select=document.getElementById(selecttag) ;

        if (select) {

          for (i=0; i<json.scripts.length; i++) {

            if ( !selecthasoption(select, json.scripts[i]) ) {

              var option=document.createElement("option") ;
              option.text="GET/PUT /" + json.scripts[i] ;
              option.value= json.scripts[i] ; 
              select.add(option) ;

            }

          }
    
        }

      } 

      if (json && json.media) {
    
        // add to media list

        var medialist=document.getElementById(mediatag) ;

        if (medialist) {

          var lst = "" ;

          for (var key in json.media) {
            if (json.media.hasOwnProperty(key)) {
              lst = lst + "<li>" + json.media[key] + "</li>" ;
            }
          }

          medialist.innerHTML = "<p><b>Test Media</b></p><ul>" + lst + "</ul>" ;

        }

      }

    } catch (e) {

      console.log("error accessing / parsing /serverinfo json") ;

    }
    
  }) ;

}


function selecthasoption(select, value)
{
  if ( !select || !select.options || !select.options.length ) return false ; 

  var len = select.options.length ;

  for (var i=0; i<len; i++) {

    if (select.options[i].value == value) return true ;

  }

  return false ;
}


function getsampleerror(json, errmsg)
{
  var str = "" + errmsg ;
  var matches = str.match(/\d+/g);
  var errpos = -1 ;
  if (matches) errpos = parseInt(0 + matches[0]) ;
  var sample="" ;

  if (errpos>0) {
    var from = errpos-60 ;
    var to = errpos+60 ;
    if (from<0) from=0 ;
    if (to>json.length) to=json.length;

    sample = json.substring(from,errpos) + 
             "<b><u>&nbsp;" + json.substring(errpos,errpos+1) + "&nbsp;</u></b>" + 
             json.substring(errpos+1, to) + "<br/>"  ;

  }

  return "<p>" + str + "</p><p>" + sample + "</p>" ;
}


