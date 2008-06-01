/*
 a test file for Random classes
*/

#include <assert.h>
#include <ACG.h>
#include <MLCG.h>
#include <SmplStat.h>
#include <SmplHist.h>
#include <Binomial.h>
#include <Erlang.h>
#include <Geom.h>
#include <HypGeom.h>
#include <NegExp.h>
#include <Normal.h>
#include <LogNorm.h>
#include <Poisson.h>
#include <Uniform.h>
#include <DiscUnif.h>
#include <Weibull.h>

void demo(Random& r)
{
  SampleStatistic s;
  cout << "five samples:\n";
  for (int i = 0; i < 5; ++i)
  {
    double x = r();
    cout << x << " ";
    s += x;
  }
  cout << "\nStatistics for 100 samples:\n";
  for (; i < 100; ++i)
  {
    double x = r();
    s += x;
  }
  cout << "samples: " << s.samples() << " ";
  cout << "min: " << s.min() << " ";
  cout << "max: " << s.max() << "\n";
  cout << "mean: " << s.mean() << " ";
  cout << "stdDev: " << s.stdDev() << " ";
  cout << "var: " << s.var() << " ";
  cout << "confidence(95): " << s.confidence(95) << "\n";
}

main()
{
  int i;
  ACG gen1;
  cout << "five random ACG integers:\n";
  for (i = 0; i < 5; ++i)
    cout << gen1.asLong() << " ";
  cout << "\n";

  MLCG gen2;
  cout << "five random MLCG integers:\n";
  for (i = 0; i < 5; ++i)
    cout << gen2.asLong() << " ";
  cout << "\n";
  
  Binomial r1( 100, 0.5, &gen1);
  cout << "Binomial r1( 100, 0.50, &gen1) ...\n";
  demo(r1);
  Erlang r2( 2.0, 0.5, &gen1);
  cout << "Erlang r2( 2.0, 0.5, &gen1) ...\n";
  demo(r2);
  Geometric r3( 0.5, &gen1);
  cout << "Geometric r3(&gen1, 0.5)...\n";
  demo(r3);
  HyperGeometric r4( 10.0, 150.0, &gen1);
  cout << "HyperGeometric r4( 10.0, 150.0, &gen1)...\n";
  demo(r4);
  NegativeExpntl r5( 1.0, &gen1);
  cout << "NegativeExpntl r5( 1.0, &gen1)...\n";
  demo(r5);
  Normal r6( 0.0, 1.0, &gen1);
  cout << "Normal r6( 0.0, 1.0, &gen1)...\n";
  demo(r6);
  LogNormal r7( 1.0, 1.0, &gen1);
  cout << "LogNormal r7( 1.0, 1.0, &gen1)...\n";
  demo(r7);
  Poisson r8( 2.0, &gen1);
  cout << "Poisson r8( 2.0, &gen1)...\n";
  demo(r8);
  DiscreteUniform r9( 0, 1, &gen1);
  cout << "DiscreteUniform r9( 0.0, 1.0, &gen1)...\n";
  demo(r9);
  Uniform r10( 0.0, 1.0, &gen1);
  cout << "Uniform r10( 0.0, 1.0, &gen1)...\n";
  demo(r10);
  Weibull r11( 0.5, 1.0, &gen1);
  cout << "Weibull r11( 0.5, 1.0, &gen1)...\n";
  demo(r11);

  cout << "SampleHistogram for 100 Normal samples\n";
  SampleHistogram h(-4.0, 4.0);
  for (i = 0; i < 100; ++i)
    h += r6();
  h.printBuckets(cout);
  cout << "\nEnd of test\n";
  return 0;
}
