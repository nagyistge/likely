; ModuleID = 'likely'
source_filename = "likely"

%u0Matrix = type { i32, i32, i32, i32, i32, i32, [0 x i8] }
%u16Matrix = type { i32, i32, i32, i32, i32, i32, [0 x i16] }

; Function Attrs: nounwind
declare void @llvm.assume(i1) #0

; Function Attrs: argmemonly nounwind
declare noalias %u0Matrix* @likely_new(i32 zeroext, i32 zeroext, i32 zeroext, i32 zeroext, i32 zeroext, i8* noalias nocapture) #1

; Function Attrs: norecurse nounwind
define private void @convert_grayscale_tmp_thunk0({ %u16Matrix*, %u16Matrix* }* noalias nocapture readonly, i64, i64) #2 {
entry:
  %3 = getelementptr inbounds { %u16Matrix*, %u16Matrix* }, { %u16Matrix*, %u16Matrix* }* %0, i64 0, i32 0
  %4 = load %u16Matrix*, %u16Matrix** %3, align 8
  %5 = getelementptr inbounds { %u16Matrix*, %u16Matrix* }, { %u16Matrix*, %u16Matrix* }* %0, i64 0, i32 1
  %6 = load %u16Matrix*, %u16Matrix** %5, align 8
  %7 = getelementptr inbounds %u16Matrix, %u16Matrix* %6, i64 0, i32 3
  %columns1 = load i32, i32* %7, align 4, !range !0
  %dst_y_step = zext i32 %columns1 to i64
  %8 = getelementptr inbounds %u16Matrix, %u16Matrix* %6, i64 0, i32 2
  %channels = load i32, i32* %8, align 4, !range !0
  %src_c = zext i32 %channels to i64
  %9 = mul nuw nsw i64 %dst_y_step, %2
  br label %y_body

y_body:                                           ; preds = %y_body, %entry
  %y = phi i64 [ %1, %entry ], [ %y_increment, %y_body ]
  %10 = mul nuw nsw i64 %y, %src_c
  %11 = getelementptr %u16Matrix, %u16Matrix* %6, i64 0, i32 6, i64 %10
  %12 = load i16, i16* %11, align 2, !llvm.mem.parallel_loop_access !1
  %13 = getelementptr %u16Matrix, %u16Matrix* %6, i64 0, i32 6, i64 %10
  %14 = getelementptr i16, i16* %13, i64 1
  %15 = load i16, i16* %14, align 2, !llvm.mem.parallel_loop_access !1
  %16 = getelementptr %u16Matrix, %u16Matrix* %6, i64 0, i32 6, i64 %10
  %17 = getelementptr i16, i16* %16, i64 2
  %18 = load i16, i16* %17, align 2, !llvm.mem.parallel_loop_access !1
  %19 = zext i16 %12 to i32
  %20 = mul nuw nsw i32 %19, 1868
  %21 = zext i16 %15 to i32
  %22 = mul nuw nsw i32 %21, 9617
  %23 = zext i16 %18 to i32
  %24 = mul nuw nsw i32 %23, 4899
  %25 = add nuw nsw i32 %20, 8192
  %26 = add nuw nsw i32 %25, %22
  %27 = add nuw i32 %26, %24
  %28 = lshr i32 %27, 14
  %29 = getelementptr %u16Matrix, %u16Matrix* %4, i64 0, i32 6, i64 %y
  %30 = trunc i32 %28 to i16
  store i16 %30, i16* %29, align 2, !llvm.mem.parallel_loop_access !1
  %y_increment = add nuw nsw i64 %y, 1
  %y_postcondition = icmp eq i64 %y_increment, %9
  br i1 %y_postcondition, label %y_exit, label %y_body

y_exit:                                           ; preds = %y_body
  ret void
}

declare void @likely_fork(i8* noalias nocapture, i8* noalias nocapture, i64)

; Function Attrs: nounwind
define noalias %u16Matrix* @convert_grayscale(%u16Matrix* noalias nocapture) #0 {
entry:
  %1 = getelementptr inbounds %u16Matrix, %u16Matrix* %0, i64 0, i32 3
  %columns = load i32, i32* %1, align 4, !range !0
  %2 = getelementptr inbounds %u16Matrix, %u16Matrix* %0, i64 0, i32 4
  %rows = load i32, i32* %2, align 4, !range !0
  %3 = call %u0Matrix* @likely_new(i32 25616, i32 1, i32 %columns, i32 %rows, i32 1, i8* null)
  %dst = bitcast %u0Matrix* %3 to %u16Matrix*
  %4 = zext i32 %rows to i64
  %5 = alloca { %u16Matrix*, %u16Matrix* }, align 8
  %6 = bitcast { %u16Matrix*, %u16Matrix* }* %5 to %u0Matrix**
  store %u0Matrix* %3, %u0Matrix** %6, align 8
  %7 = getelementptr inbounds { %u16Matrix*, %u16Matrix* }, { %u16Matrix*, %u16Matrix* }* %5, i64 0, i32 1
  store %u16Matrix* %0, %u16Matrix** %7, align 8
  %8 = bitcast { %u16Matrix*, %u16Matrix* }* %5 to i8*
  call void @likely_fork(i8* bitcast (void ({ %u16Matrix*, %u16Matrix* }*, i64, i64)* @convert_grayscale_tmp_thunk0 to i8*), i8* %8, i64 %4) #0
  ret %u16Matrix* %dst
}

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
attributes #2 = { norecurse nounwind }

!0 = !{i32 1, i32 -1}
!1 = distinct !{!1}
