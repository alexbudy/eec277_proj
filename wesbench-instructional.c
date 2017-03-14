#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* user settable define's */

/*
 * setting SCREENSHOT_MODE to 1 will cause the triangle benchmark code to
 * render one frame, pause for 5 seconds, then exit.  A value of 0 will
 * result in normal benchmark runs.
 */
#define SCREENSHOT_MODE 0

/*
 * choose an internal texture storage format. This one could be specified by a
 * command-line argument.
 */
#define TEXTURE_STORAGE_MODE GL_RGBA8
/*#define TEXTURE_STORAGE_MODE GL_R3_G3_B2 */


#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "util.h"

void Init (void);
void printInfo (GLFWwindow * window);
void runBenchmark (void);
void Reshape (int, int);
void Key (unsigned char, int, int);
void check_gl_errors (void);


typedef enum
  {
    DISJOINT_TRIANGLES = 0x00,
  } TriangleType;

#define DEFAULT_TRIANGLE_AREA 128.0
#define DEFAULT_TEST_DURATION_SECONDS 5.0
#define DEFAULT_WIN_WIDTH 1024
#define DEFAULT_WIN_HEIGHT 1024
#define DEFAULT_TRIANGLE_LIMIT 1024*1024*1024 /* 1G */
#define DEFAULT_VERTEXBUF_LIMIT 1024*1024*1024 /* 1G */
#define DEFAULT_DUMP_FILE_NAME "wesBench-dump.txt"
#define DEFAULT_RETAINED_MODE_ENABLED  0
#define DEFAULT_TRIANGLE_TYPE DISJOINT_TRIANGLES;
#define DEFAULT_CLEAR_PER_FRAME 0
#define DEFAULT_OUTLINE_MODE_BOOL 1 /* 0 means draw filled tri's, 1 means outline */

typedef struct
{
  char  *appName;             /* obtained from argv[0] */
  double triangleAreaInPixels; /* set by command line arg -a XXX */
  double testDurationSeconds; /* set by command line arg -s NNNN */
  int    imgWidth, imgHeight; /* set by -w WWW -h HHH -- NOT IMPLEMENTED */

  size_t triangleLimit;       /* set by -tl NNNN  */
  size_t vertexBufLimit;      /* set by -vl NNNN */


  int    outlineMode;         /* set by -line */
  TriangleType  triangleType;


  float  computedFPS;
  float  computedTrisPerSecond;
  float  computedMVertexOpsPerSecond;
  float  computedMFragsPerSecond;
  float  computedMTrisPerSecond;
  size_t computedVertsPerArrayCall; /* the number of verts issued in a glDrawArrays call */
  size_t computedIndicesPerArrayCall; /* the number of indices in glDrawElements call*/
} AppState;

/* global: I hate globals. GLUT doesn't appear to have a mechanism to
   store/retrieve client data, so we're stuck with a global.  */
AppState myAppState;

void wesTriangleRateBenchmark(AppState *myAppState);


typedef struct
{
  GLfloat x, y;
} Vertex2D;

typedef struct
{
  GLfloat x, y, z;
} Vertex3D;

typedef struct
{
  GLfloat r, g, b;
} Color3D;

typedef struct
{
  unsigned char r, g, b, a;
} Color4DUbyte;


const char usageString[] =
  {" \
[-a AAAA]\tsets triangle area in pixels (double precision value)\n \
[-tl LLLL]\tsets maximum number of triangles (long int value)\n \
[-s NNNN]\tsets the duration of the test in seconds.\n \
[-w WWW -h HHH]\t sets the display window size.\n \
[-df fname] sets the name of the dumpfile for performance statistics.\n \
[-tt (0, 1, 2, 3)]\tset triangle type: 0=disjoint, 1=tstrip, 2=indexed disjoint, 3=indexed tstrip\n \
[-line]\tSet polygon mode to GL_LINE to draw triangle outlines, no fill. \n \
\n"};

void
parseArgs(int argc,
          char **argv,
          AppState *myAppState)
{
  int i=1;
  argc--;
  while (argc > 0)
    {
      if (strcmp(argv[i],"-a") == 0)
        {
          i++;
          argc--;
          myAppState->triangleAreaInPixels = atof(argv[i]);
        }
      else if (strcmp(argv[i],"-l") == 0)
        {
          i++;
          argc--;
          myAppState->triangleLimit = (size_t)atol(argv[i]);
        }
      else if (strcmp(argv[i],"-s") == 0)
        {
          i++;
          argc--;
          myAppState->testDurationSeconds = atof(argv[i]);
        }
      else if (strcmp(argv[i],"-tl") == 0)
        {
          i++;
          argc--;
          myAppState->triangleLimit = atoi(argv[i]);
        }
      else if (strcmp(argv[i],"-vl") == 0)
        {
          i++;
          argc--;
          myAppState->vertexBufLimit = atoi(argv[i]);
        }
      else if (strcmp(argv[i], "-line") == 0)
        {
          myAppState->outlineMode = 1;
        }
      else
        {
          fprintf(stderr,"Unrecognized argument: %s \n", argv[i]);
          exit(-1);
        }
      i++;
      argc--;
    }
}

void normalizeNormal(Vertex3D *n)
{
  double d = n->x*n->x + n->y*n->y + n->z*n->z;
  d = sqrt(d);
  if (d != 0.0)
    d = 1.0/d;
  n->x *= d;
  n->y *= d;
  n->z *= d;
}

void
buildDisjointTriangleArrays(int nVertsPerAxis,
                            int triangleLimit,
                            int *dispatchTriangles,
                            int *dispatchVertexCount,
                            Vertex2D *baseVerts,
                            Color3D *baseColors,
                            Vertex3D *baseNormals,
                            Vertex2D *baseTCs,
                            Vertex2D **dispatchVerts,
                            Color3D **dispatchColors,
                            Vertex3D **dispatchNormals,
                            Vertex2D **dispatchTCs)
{

  /* now, create and populate the arrays that will be used to dispatch
     triangle data off to OpenGL */

  int i, j, sIndx, dIndx;
  Vertex2D *dv, *dtc;
  Color3D *dc;
  Vertex3D *dn;
  *dispatchTriangles = nVertsPerAxis*nVertsPerAxis*2;

  dv = *dispatchVerts = (Vertex2D *)malloc(sizeof(Vertex2D)*(*dispatchTriangles)*3);
  dc = *dispatchColors = (Color3D *)malloc(sizeof(Color3D)*(*dispatchTriangles)*3);
  dn = *dispatchNormals = (Vertex3D *)malloc(sizeof(Vertex3D)*(*dispatchTriangles)*3);
  dtc = *dispatchTCs = (Vertex2D *)malloc(sizeof(Vertex2D)*(*dispatchTriangles)*3);

  dIndx = 0;
  for (j=0;j<nVertsPerAxis;j++)
    {
      for (i=0;i<nVertsPerAxis;i++)
        {
          /* for each mesh quad, copy over verts for two triangles */
          sIndx = j*(nVertsPerAxis+1) + i;

          /* triangle 0 */
          dv[dIndx] = baseVerts[sIndx];
          dc[dIndx] = baseColors[sIndx];
          dn[dIndx] = baseNormals[sIndx];
          dtc[dIndx] = baseTCs[sIndx];
          dIndx++;

          dv[dIndx] = baseVerts[sIndx+1];
          dc[dIndx] = baseColors[sIndx+1];
          dn[dIndx] = baseNormals[sIndx+1];
          dtc[dIndx] = baseTCs[sIndx+1];
          dIndx++;

          dv[dIndx] = baseVerts[sIndx+nVertsPerAxis+1];
          dc[dIndx] = baseColors[sIndx+nVertsPerAxis+1];
          dn[dIndx] = baseNormals[sIndx+nVertsPerAxis+1];
          dtc[dIndx] = baseTCs[sIndx+nVertsPerAxis+1];
          dIndx++;

          /* triangle 1 */
          dv[dIndx] = baseVerts[sIndx+nVertsPerAxis+1];
          dc[dIndx] = baseColors[sIndx+nVertsPerAxis+1];
          dn[dIndx] = baseNormals[sIndx+nVertsPerAxis+1];
          dtc[dIndx] = baseTCs[sIndx+nVertsPerAxis+1];
          dIndx++;

          dv[dIndx] = baseVerts[sIndx+1];
          dc[dIndx] = baseColors[sIndx+1];
          dn[dIndx] = baseNormals[sIndx+1];
          dtc[dIndx] = baseTCs[sIndx+1];
          dIndx++;

          dv[dIndx] = baseVerts[sIndx+1+nVertsPerAxis+1];
          dc[dIndx] = baseColors[sIndx+1+nVertsPerAxis+1];
          dn[dIndx] = baseNormals[sIndx+1+nVertsPerAxis+1];
          dtc[dIndx] = baseTCs[sIndx+1+nVertsPerAxis+1];
          dIndx++;
        }
    }

  /*
   * now we have a big pile of disjoint triangles. how many do we
   * actually send down with each call?
   */
  if (*dispatchTriangles < triangleLimit)
    *dispatchVertexCount = *dispatchTriangles*3;
  else
    *dispatchVertexCount = triangleLimit*3;
  *dispatchTriangles = *dispatchVertexCount/3;
}


void
buildBaseArrays(float triangleAreaPixels,
                int screenWidth,
                int screenHeight,
                int *nVertsPerAxis,
                Vertex2D **baseVerts,
                Color3D **baseColors,
                Vertex3D **baseNormals,
                Vertex2D **baseTCs)
{
  /* construct base arrays for qmesh */

  int usablePixels = screenWidth >> 1;
  int i,j, indx=0;
  float x, y;
  float r, g, dr, dg;
  float s, ds, t, dt;
  Vertex2D *bv, *btc;
  Vertex3D *bn;
  Color3D *bc;

  /*
   * we're going to construct a mesh that has vertex spacing
   * at an interval such that each mesh quad will have two
   * triangles whose area sums to triangleAreaPixels*2.
   */
  double spacing = sqrt(triangleAreaPixels*2.0);
  *nVertsPerAxis = (int)((double)usablePixels/spacing);

  bv = *baseVerts = (Vertex2D *)malloc(sizeof(Vertex2D)*(*nVertsPerAxis+1)*(*nVertsPerAxis+1));
  bc = *baseColors = (Color3D *)malloc(sizeof(Color3D)*(*nVertsPerAxis+1)*(*nVertsPerAxis+1));
  bn = *baseNormals = (Vertex3D *)malloc(sizeof(Vertex3D)*(*nVertsPerAxis+1)*(*nVertsPerAxis+1));
  btc = *baseTCs = (Vertex2D *)malloc(sizeof(Vertex2D)*(*nVertsPerAxis+1)*(*nVertsPerAxis+1));

  y= 0.25F * screenWidth;
  g = 0.0F;

  dg = dr = 1.0F/(float)*nVertsPerAxis;

  t = 0.0F;
  dt = ds = 1.0/(float)*nVertsPerAxis;

  /* red grows along x axis, green along y axis, b=1 constant */

  for (j=0;j<*nVertsPerAxis+1;j++,y+=spacing, g+=dg, t+=dt)
    {
      x = 0.25 * screenWidth;
      r = 0.0;
      s = 0.0F;

      for (i=0;i<*nVertsPerAxis+1;i++, x+=spacing, r+=dr, s+=ds)
        {
          bv[indx].x = x;
          bv[indx].y = y;

          bc[indx].r = r;
          bc[indx].g = g;
          bc[indx].b = 1.0F;

          bn[indx].x = x-(float)screenWidth*0.5;
          bn[indx].y = y-(float)screenHeight*0.5;
          bn[indx].z = (float)(screenWidth+screenHeight)*.25;
          normalizeNormal(bn+indx);

          btc[indx].x = s;
          btc[indx].y = t;

          indx++;
        }
    }
}

void
wesTriangleRateBenchmark(AppState *as)
{
  int startTime, endTime;
  size_t totalTris=0, totalVerts=0;
  float elapsedTimeSeconds, trisPerSecond, vertsPerSecond;
  Vertex2D *baseVerts;
  Color3D *baseColors;
  Vertex3D *baseNormals;
  Vertex2D *baseTCs;
  GLuint trianglesListIndx = 0;

  int nVertsPerAxis;
  int nFrames=0;

  Vertex2D *dispatchVerts=NULL;
  Color3D *dispatchColors=NULL;
  Vertex3D *dispatchNormals=NULL;
  Vertex2D *dispatchTCs=NULL;
  GLuint   *dispatchIndices=NULL;
  int dispatchVertexCount, dispatchTriangles;

  int screenWidth = as->imgWidth;
  GLfloat r=screenWidth;
  int screenHeight = as->imgHeight;
  int testDurationSeconds = as->testDurationSeconds;
  size_t triangleLimit = as->triangleLimit;
  double triangleAreaPixels = as->triangleAreaInPixels;


  /*
   * Objective(s):
   * 1. Want triangles with a specified area (e.g., 8sq pixels) for the
   * purpose of generating a graphics load with reasonably precise
   * characteristics .
   * 2. Want triangles to *all* remain on-screen so that triangle rate
   * reflects the cost of rendering visible triangles/fragments.
   *
   * Approach:
   * 1. build a base mesh of vertices. This set of verts is positioned
   * so that it will remain entirely within the view frustum no matter
   * how we rotate it (in 2D)
   * 2. dispatch off the mesh vertices as disjoint triangles using
   * vertex arrays. This approach is not the most efficient way to
   * represent densely packed triangles, but has a couple of benefits:
   * - It allows us to easily set the maximum number of triangles per
   *   frame to be dispatched off to the renderer w/o having to resort
   *   to tricks like inserting degenerate triangles in strips to connect
   *   up two disjoint strips.
   * - It allows us to glDrawArrays rather than glDrawElements. The latter
   *   uses one level of indirection, and will be less efficient.
   * - In the end, we care about vertex rate, not triangle rate.
   *
   * An early version of this code used the glDrawElements with indices.
   * Rather than insert compile-time switches, this old code is simply
   * cut-n-pasted and commented out at the end of this file.
   */

  /* Make sure we are drawing to the front buffer */
  glDrawBuffer(GL_FRONT);
  glDisable(GL_DEPTH_TEST);

  /* Clear the screen */
  glClear(GL_COLOR_BUFFER_BIT);

  if (as->outlineMode != 0)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  else
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  /* Load identities */
  glMatrixMode( GL_PROJECTION );
  glPushMatrix();
  glLoadIdentity( );

  glMatrixMode( GL_MODELVIEW );
  glPushMatrix();
  glLoadIdentity();

  /* change range from -1..1 to 0..[screenWidth, screenHeight] */
  glTranslatef(-1.0, -1.0, 0.0);
  glScalef(2.0/r, 2.0/r, 1.0F);


  glDisable(GL_LIGHTING);

  glDisable(GL_TEXTURE_2D);

  /* build the base quadmesh vertex array */
  buildBaseArrays(triangleAreaPixels, as->imgWidth, as->imgHeight,
                  &nVertsPerAxis,
                  &baseVerts, &baseColors, &baseNormals, &baseTCs);

  /* now, repackage that information into bundles suitable for submission
     to GL using the specified primitive type*/
  if (as->triangleType == DISJOINT_TRIANGLES)
    {
      int triangleLimit;

      if ((as->triangleLimit*3) > as->vertexBufLimit)
        triangleLimit = as->vertexBufLimit/3;
      else
        triangleLimit = as->triangleLimit;

      buildDisjointTriangleArrays(nVertsPerAxis,
                                  triangleLimit,
                                  &dispatchTriangles,
                                  &dispatchVertexCount,
                                  baseVerts, baseColors,
                                  baseNormals, baseTCs,
                                  &dispatchVerts,
                                  &dispatchColors,
                                  &dispatchNormals,
                                  &dispatchTCs);
      as->computedVertsPerArrayCall = dispatchVertexCount;
      as->computedIndicesPerArrayCall = 0;
    }

  /* Set up the pointers */
  glVertexPointer(2, GL_FLOAT, 0, (const GLvoid *)dispatchVerts);
  glEnableClientState(GL_VERTEX_ARRAY);
  glColorPointer(3, GL_FLOAT, 0, (const GLvoid *)dispatchColors);
  glEnableClientState(GL_COLOR_ARRAY);

  glFinish();                 /* make sure all setup is finished */

  startTime = endTime = glfwGetTime();

  while ((endTime - startTime) < testDurationSeconds)
    {

      glDrawArrays(GL_TRIANGLES, 0, dispatchVertexCount);

      glTranslatef(r/2.0, r/2.0, 0.0F);
      glRotatef(0.01F, 0.0F, 0.0F, 1.0F);
      glTranslatef(-r/2.0, -r/2.0, 0.0F);

      endTime = glfwGetTime();

      nFrames++;
      totalTris += dispatchTriangles;
      totalVerts += as->computedVertsPerArrayCall;

    }

  glFinish();
  endTime = glfwGetTime();

  /* Restore the gl stack */
  glMatrixMode( GL_MODELVIEW );
  glPopMatrix();

  glMatrixMode( GL_PROJECTION );
  glPopMatrix();

  /* Before printing the results, make sure we didn't have
  ** any GL related errors. */
  check_gl_errors();

  /* the pause that refreshes -- */
#ifdef _WIN32
  Sleep(250);
#else
  usleep(250000);
#endif

  /* TODO: compute/return the mFrags/sec */

  elapsedTimeSeconds = endTime - startTime;
  as->computedMTrisPerSecond = ((totalTris)/1000000.0f)/elapsedTimeSeconds;
  as->computedMVertexOpsPerSecond = ((totalVerts) / 1000000.0f) / elapsedTimeSeconds;
  as->computedFPS = (nFrames + 1.0f) / elapsedTimeSeconds;
  as->computedMFragsPerSecond = as->computedMTrisPerSecond*as->triangleAreaInPixels;

  printf("verts/frame = %d \n", dispatchVertexCount);
  printf("nframes = %d \n", nFrames);

  free((void *)baseVerts);
  free((void *)baseColors);
  free((void *)baseNormals);
  free((void *)baseTCs);
  free((void *)dispatchVerts);
  free((void *)dispatchColors);
  free((void *)dispatchNormals);
  free((void *)dispatchTCs);

  if (dispatchIndices != NULL)
    free((void *)dispatchIndices);
}


/* A simple routine which checks for GL errors. */
void check_gl_errors (void) {
  GLenum err;
  err = glGetError();
  if (err != GL_NO_ERROR) {
    fprintf (stderr, "GL Error: ");
    switch (err) {
    case GL_INVALID_ENUM:
      fprintf (stderr, "Invalid Enum\n");
      break;
    case GL_INVALID_VALUE:
      fprintf (stderr, "Invalid Value\n");
      break;
    case GL_INVALID_OPERATION:
      fprintf (stderr, "Invalid Operation\n");
      break;
    case GL_STACK_OVERFLOW:
      fprintf (stderr, "Stack Overflow\n");
      break;
    case GL_STACK_UNDERFLOW:
      fprintf (stderr, "Stack Underflow\n");
      break;
    case GL_OUT_OF_MEMORY:
      fprintf (stderr, "Out of Memory\n");
      break;
    default:
      fprintf (stderr, "Unknown\n");
    }
    exit(1);
  }
}

void Init (void) {
  /* Init does nothing */
}

void Reshape (int x, int y) {
  /* Reshape does nothing */
  (void) x;
  (void) y;
}


void Key(unsigned char a, int x, int y) {
  /* Key does nothing */
  (void) x;
  (void) y;
  (void) a;
}

static void error_callback(int error, const char* description)
{
  fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}


static GLuint make_shader(GLenum type, const char *filename)
{
    GLint length;
    GLchar *source = file_contents(filename, &length);
    GLuint shader;
    GLint shader_ok;

    if (!source)
        return 0;

    shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar**)&source, &length);
    free(source);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_ok);
    if (!shader_ok) {
        fprintf(stderr, "Failed to compile %s:\n", filename);
        //show_info_log(shader, glGetShaderiv, glGetShaderInfoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}



int
main(int argc, char **argv)
{
  myAppState.appName = strdup(argv[0]);

  myAppState.triangleAreaInPixels=DEFAULT_TRIANGLE_AREA;
  myAppState.triangleType = DEFAULT_TRIANGLE_TYPE;
  myAppState.testDurationSeconds=DEFAULT_TEST_DURATION_SECONDS;
  myAppState.imgWidth = DEFAULT_WIN_WIDTH;
  myAppState.imgHeight = DEFAULT_WIN_HEIGHT;
  myAppState.triangleLimit = DEFAULT_TRIANGLE_LIMIT;
  myAppState.vertexBufLimit = DEFAULT_VERTEXBUF_LIMIT;
  myAppState.outlineMode = DEFAULT_OUTLINE_MODE_BOOL;

  glfwSetErrorCallback(error_callback);

  if (!glfwInit())
    exit(EXIT_FAILURE);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  parseArgs(argc, argv, &myAppState);

  GLFWwindow* window = glfwCreateWindow(DEFAULT_WIN_WIDTH, DEFAULT_WIN_HEIGHT, argv[0], NULL, NULL);
  if (!window)
    {
      glfwTerminate();
      exit(EXIT_FAILURE);
    }

  glfwSetKeyCallback(window, key_callback);

  glfwMakeContextCurrent(window);
  // start GLEW extension handler
  // glewExperimental = GL_TRUE;
  glewInit();
  glfwSwapInterval(1);

  printInfo(window);

  GLuint program = glCreateProgram();
  GLuint vertexshader = make_shader(GL_VERTEX_SHADER, "hello-gl.v.glsl");
  GLuint fragmentshader = make_shader(GL_FRAGMENT_SHADER, "hello-gl.f.glsl");
  glAttachShader(program, vertexshader);
  glAttachShader(program, fragmentshader);
  glLinkProgram(program);

  Init();
  while (!glfwWindowShouldClose(window))
    {
      glUseProgram(program);
      runBenchmark();
      glfwSwapBuffers(window);
      glfwPollEvents();
    }
  return 0;
}

void printInfo (GLFWwindow * window) {

  /* Display the gfx card information */
  printf( "--------------------------------------------------\n");
  printf ("Vendor:      %s\n", glGetString(GL_VENDOR));
  printf ("Renderer:    %s\n", glGetString(GL_RENDERER));
  printf ("Version:     %s\n", glGetString(GL_VERSION));
  GLFWmonitor * monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode * vidMode = glfwGetVideoMode(monitor);
  printf( "Visual:      RGBA=<%d,%d,%d,%d>  Z=<%d>  double=%d\n",
          vidMode->redBits,
          vidMode->blueBits,
          vidMode->greenBits,
          GLFW_ALPHA_BITS,
          GLFW_DEPTH_BITS,
          1 );
  int winWidth, winHeight;
  glfwGetFramebufferSize(window, &winWidth, &winHeight);
  int xPos, yPos;
  glfwGetWindowPos(window, &xPos, &yPos);
  printf( "Geometry:    %dx%d+%d+%d\n",
          winWidth,
          winHeight,
          xPos,
          yPos);
  printf( "Screen:      %dx%d\n",
          vidMode->width,
          vidMode->height);
  printf( "--------------------------------------------------\n");


  /* print out the test parameters */

  printf("Triangle area\t%4.3f (pixels^2)\n", myAppState.triangleAreaInPixels);
  printf("Test duration\t%f(s)\n", myAppState.testDurationSeconds);
  printf("Screen W/H\t(%d,%d)\n", myAppState.imgWidth, myAppState.imgHeight);
  printf("Triangle limit\t%ld\n", myAppState.triangleLimit);
  printf("VertexBuf limit\t%ld\n", myAppState.vertexBufLimit);
  printf("Triangle type\t%d \n", myAppState.triangleType);

}

void runBenchmark(void) {

      wesTriangleRateBenchmark(&myAppState);

      fprintf(stderr," WesBench: area=%2.1f px, tri rate = %3.2f Mtri/sec, vertex rate=%3.2f Mverts/sec, fill rate = %4.2f Mpix/sec, verts/bucket=%zu, indices/bucket=%zu\n", myAppState.triangleAreaInPixels, myAppState.computedMTrisPerSecond, myAppState.computedMVertexOpsPerSecond, myAppState.computedMFragsPerSecond, myAppState.computedVertsPerArrayCall, myAppState.computedIndicesPerArrayCall);

  glfwTerminate();
  exit(0);
}
/* EOF */
