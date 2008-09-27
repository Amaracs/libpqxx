/* main() definition for libpqxx test runners.
 */
#include <iostream>
#include <new>
#include <stdexcept>

#include "pqxx/except"
#include "pqxx/util"


using namespace PGSTD;
using namespace pqxx;
using namespace pqxx::test;


namespace
{
// Does this backend have generate_series()?
bool have_generate_series(const connection_base &c)
{
  return c.server_version() >= 80000;
}
} // namespace


namespace pqxx
{
namespace test
{
test_failure::test_failure(const string &ffile, int fline, const string &desc) :
  logic_error(desc),
  m_file(ffile),
  m_line(fline)
{
}


test_failure::~test_failure() throw () {}


base_test::base_test(const string &tname, testfunc func) :
  m_name(tname),
  m_func(func)
{
  register_test(this);
}


base_test::~base_test() {}

const test_map &register_test(base_test *tc)
{
  static test_map tests;
  if (tc) tests[tc->name()] = tc;
  return tests;
}

void prepare_series(transaction_base &t, int lowest, int highest)
{
  connection_base &conn = t.conn();
  // Don't do this for nullconnections, so nullconnection tests can run.
  if (conn.is_open() && !have_generate_series(conn))
  {
    t.exec("CREATE TEMP TABLE series(x integer)");
    for (int x=lowest; x <= highest; ++x)
      t.exec("INSERT INTO series(x) VALUES (" + to_string(x) + ")");
  }
}


PGSTD::string select_series(connection_base &conn, int lowest, int highest)
{
  if (have_generate_series(conn))
    return
	"SELECT generate_series(" +
	to_string(lowest) + ", " + to_string(highest) + ")";

  return
	"SELECT x FROM series "
	"WHERE "
	  "x >= " + to_string(lowest) + " AND "
	  "x <= " + to_string(highest) + " "
	"ORDER BY x";
}


void check_notreached(const char file[], int line, PGSTD::string desc)
{
  throw test_failure(file, line, desc);
}


void check(
	const char file[],
	int line,
	bool condition,
	const char text[], 
	PGSTD::string desc)
{
  if (!condition)
    throw test_failure(
	file,
	line,
	desc + " (failed expression: " + text + ")");
}

} // namespace pqxx::test
} // namespace pqxx


int main(int, const char *argv[])
{
  const char *const test_name = argv[1];
  const test_map &tests = register_test(NULL);

  int test_count = 0, failures = 0;
  for (test_map::const_iterator i = tests.begin(); i != tests.end(); ++i)
    if (!test_name || test_name == i->first)
    {
      PGSTD::cout << PGSTD::endl << "Running: " << i->first << PGSTD::endl;

      bool success = false;
      try
      {
        i->second->run();
        success = true;
      }
      catch (const test_failure &e)
      {
        PGSTD::cerr << "Test failure in " + e.file() + " line " + 
	    to_string(e.line()) << ": " << e.what() << PGSTD::endl;
      }
      catch (const PGSTD::bad_alloc &)
      {
        PGSTD::cerr << "Out of memory!" << PGSTD::endl;
      }
      catch (const sql_error &e)
      {
        PGSTD::cerr << "SQL error: " << e.what() << PGSTD::endl
             << "Query was: " << e.query() << PGSTD::endl;
      }
      catch (const PGSTD::exception &e)
      {
        PGSTD::cerr << "Exception: " << e.what() << PGSTD::endl;
      }
      catch (...)
      {
        PGSTD::cerr << "Unknown exception" << PGSTD::endl;
      }

      if (!success) ++failures;
      ++test_count;
    }

  PGSTD::cout << "Ran " << test_count << " test(s)." << PGSTD::endl;

  if (failures > 0)
    PGSTD::cout << "*** " << failures << " test(s) failed. ***" << PGSTD::endl;

  return failures;
}