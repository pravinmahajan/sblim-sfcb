require 'test/unit'
require 'net/http'
require 'uri'
require File.join(File.dirname(__FILE__), "sfcb.rb")

class StartStopTest < Test::Unit::TestCase
  def setup
    @sfcb = Sfcb.new
    @sfcb.start
    sleep 3
    puts "\n\t*** Started"
    @http = Net::HTTP.new "localhost", "#{@sfcb.port}"
  end
  def teardown
    puts "\n\t*** Stopping"
    @sfcb.stop
  end
  
  def test_get_root
    resp = @http.get "/cimrs/"
#    assert resp
#  end

#  def test_get_namespaces
    resp = @http.get "/cimrs/namespaces"
#    assert resp
#  end

#  def test_get_namespace_root_cimv2
    resp = @http.get "/cimrs/namespaces/root%2Fcimv2"
#    assert resp
#  end

#  def test_get_classes_root_cimv2
    resp = @http.get "/cimrs/namespaces/root%2Fcimv2/classes"
#    assert resp
  end
end
