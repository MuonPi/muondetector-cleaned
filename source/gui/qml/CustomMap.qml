import QtQuick 2.2
import QtLocation 5.7
import QtQuick.Layouts 1.0
import QtPositioning 5.0
import QtQuick.Controls 2.0
import "qrc:/qml/places/items"
//import "qrc:/qml/places/views"

CustomMapForm {
    id: page
    Plugin {
            id: plugin
            name: "osm" // "mapboxgl", "esri", ...
    }
    function setCircle(lon,lat,hAcc)
    {
        map.onCoordsReceived(lon,lat,hAcc)
    }
    MapComponent{
        id: map
        property double lastLon: 8.673828
        property double lastLat: 50.569212
        plugin: plugin
        anchors.top: parent.top
        width: parent.width
        height: parent.height
        zoomLevel: (map.maximumZoomLevel - map.minimumZoomLevel)/2
        function onCoordsReceived(lon,lat,hAcc)
        {
            lastLon = lon
            lastLat = lat
            circle.center.longitude = lon
            circle.center.latitude = lat
            circle.radius = hAcc
            if (control.checked){
                map.center = QtPositioning.coordinate(lat,lon)
            }
        }
        function jumpToLocation(){
            map.center = QtPositioning.coordinate(lastLat,lastLon)
        }

        MapCircle {
            id: circle
            center: parent.center
            radius: 0.0
            border.width: 2
        }
        ToolBar{
            id: buttonBar
            anchors.top: parent.top
            height: 30
            RowLayout{
                Rectangle{
                    height: 30
                    width: 60
                    Layout.alignment: Qt.AlignTop
                    color: "white"
                    ToolButton{
                        id: centerButton
                        anchors.fill: parent
                        text: "center"
                        onClicked: map.jumpToLocation()
                    }
                }
                Rectangle{
                    Layout.column: 1
                    width: 90
                    Layout.alignment: Qt.AlignTop
                    height: 30
                    color:"white"
                    CheckBox{
                        id: control
                        anchors.fill: parent
                        text: "follow"
                    }
                }
            }
        }
    }
}
