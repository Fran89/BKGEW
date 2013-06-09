
var map;
var marker;

function initialize(lat, lon) {

  var myOptions = {
    center:     new google.maps.LatLng(lat, lon),
    zoom:       17,
    mapTypeId:  google.maps.MapTypeId.SATELLITE,
    panControl: true
  };

  map    = new google.maps.Map(document.getElementById("map_canvas"), myOptions);

  var image = new google.maps.MarkerImage("qrc:///map/html/crosshair.png",
                                          null, 
                                          new google.maps.Point(0,0),
                                          new google.maps.Point(117,117)
                                         );
  marker = new google.maps.Marker({
    map:      map,
    position: new google.maps.LatLng(lat, lon),
    icon:     image,
  });
}

function gotoLocation(lat, lon) {
  var position = new google.maps.LatLng(lat, lon);
  map.setCenter(position);
  marker.setPosition(position);
}

