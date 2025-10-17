#!/bin/bash
# Xray 所有测试运行脚本
# 运行所有字节码VM测试

echo "========================================"
echo "   Xray Bytecode VM - All Tests"
echo "========================================"
echo ""

PASSED=0
FAILED=0

# 运行单个测试
run_test() {
    local name=$1
    local test_file=$2
    
    echo "--- Running: $name ---"
    if timeout 10 "$test_file" > /dev/null 2>&1; then
        echo "✓ $name passed"
        ((PASSED++))
    else
        echo "✗ $name failed"
        ((FAILED++))
    fi
    echo ""
}

# 基础测试
run_test "Closure Basic" "test/test_closure_bc"
run_test "Closure Advanced" "test/test_closure_advanced_bc"
run_test "Array Operations" "test/test_array_bc"
run_test "Array Assignment" "test/test_array_set_bc"
run_test "Print Function" "test/test_print_bc"
run_test "Comprehensive Features" "test/test_comprehensive_features"

# 调试和验证测试
run_test "Recursive Closure" "test/test_recursive_closure_minimal"
run_test "For Loop Zero Iterations" "test/test_for_zero"
run_test "Comparison Operators" "test/test_comp_simple"

echo "========================================"
echo "   Test Summary"
echo "========================================"
echo "   Total:  $((PASSED + FAILED))"
echo "   Passed: $PASSED"
echo "   Failed: $FAILED"
echo "========================================"

if [ $FAILED -eq 0 ]; then
    echo "   🎉 All tests passed!"
    exit 0
else
    echo "   ⚠️  Some tests failed"
    exit 1
fi

