function SetUpServer() {
  TopHeader = "<HTML><title>Walk through a maze</title>"
  TopDoc = "\
    <h2>Please choose one of the following actions:</h2>\
    <UL>\
      <LI><A HREF=" MyPrefix "/AboutServer>About this server</A>\
      <LI><A HREF=" MyPrefix "/VRMLtest>Watch a simple VRML scene</A>\
    </UL>"
  TopFooter  = "</HTML>"
  srand()
}
function HandleGET() {
  if (MENU[2] == "AboutServer") {
    Document  = "If your browser has a VRML 2 plugin,\
      this server shows you a simple VRML scene."
  } else if (MENU[2] == "VRMLtest") {
    XSIZE = YSIZE = 11              # initially, everything is wall
    for (y = 0; y < YSIZE; y++)
       for (x = 0; x < XSIZE; x++)
          Maze[x, y] = "#"
    delete Maze[0, 1]              # entry is not wall
    delete Maze[XSIZE-1, YSIZE-2]  # exit  is not wall
    MakeMaze(1, 1)
    Document = "\
#VRML V2.0 utf8\n\
Group {\n\
  children [\n\
    PointLight {\n\
      ambientIntensity 0.2\n\
      color 0.7 0.7 0.7\n\
      location 0.0 8.0 10.0\n\
    }\n\
    DEF B1 Background {\n\
      skyColor [0 0 0, 1.0 1.0 1.0 ]\n\
      skyAngle 1.6\n\
      groundColor [1 1 1, 0.8 0.8 0.8, 0.2 0.2 0.2 ]\n\
      groundAngle [ 1.2 1.57 ]\n\
    }\n\
    DEF Wall Shape {\n\
      geometry Box {size 1 1 1}\n\
      appearance Appearance { material Material { diffuseColor 0 0 1 } }\n\
    }\n\
    DEF Entry Viewpoint {\n\
      position 0.5 1.0 5.0\n\
      orientation 0.0 0.0 -1.0 0.52\n\
    }\n"
    for (i in Maze) {
      split(i, t, SUBSEP)
      Document = Document "    Transform { translation "
      Document = Document t[1] " 0 -" t[2] " children USE Wall }\n"
    }
    Document = Document "  ] # end of group for world\n}"
    Reason = "OK" ORS "Content-type: model/vrml"
    Header = Footer = ""
  }
}
function MakeMaze(x, y) {
  delete Maze[x, y]     # here we are, we have no wall here
  p = 0                 # count unvisited fields in all directions
  if (x-2 SUBSEP y   in Maze) d[p++] = "-x"
  if (x   SUBSEP y-2 in Maze) d[p++] = "-y"
  if (x+2 SUBSEP y   in Maze) d[p++] = "+x"
  if (x   SUBSEP y+2 in Maze) d[p++] = "+y"
  if (p>0) {            # if there are univisited fields, go there
    p = int(p*rand())   # choose one unvisited field at random
    if        (d[p] == "-x") { delete Maze[x - 1, y]; MakeMaze(x - 2, y)
    } else if (d[p] == "-y") { delete Maze[x, y - 1]; MakeMaze(x, y - 2)
    } else if (d[p] == "+x") { delete Maze[x + 1, y]; MakeMaze(x + 2, y)
    } else if (d[p] == "+y") { delete Maze[x, y + 1]; MakeMaze(x, y + 2)
    }                   # we are back from recursion
    MakeMaze(x, y);     # try again while there are unvisited fields
  }
}
